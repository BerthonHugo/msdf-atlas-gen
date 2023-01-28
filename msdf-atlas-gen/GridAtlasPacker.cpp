
#include "GridAtlasPacker.h"

#include <algorithm>

namespace msdf_atlas {

static int floorPOT(int x) {
    int y = 1;
    while (x >= y)
        y <<= 1;
    return y>>1;
}

static int ceilPOT(int x) {
    int y = 1;
    while (x > y)
        y <<= 1;
    return y;
}

static bool squareConstraint(DimensionsConstraint constraint) {
    switch (constraint) {
        case DimensionsConstraint::SQUARE:
        case DimensionsConstraint::EVEN_SQUARE:
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            return true;
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
            return false;
    }
    return true;
}

void GridAtlasPacker::lowerToConstraint(int &width, int &height, DimensionsConstraint constraint) {
    if (squareConstraint(constraint))
        width = height = std::min(width, height);
    switch (constraint) {
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::SQUARE:
            break;
        case DimensionsConstraint::EVEN_SQUARE:
            width &= ~1;
            height &= ~1;
            break;
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
            width &= ~3;
            height &= ~3;
            break;
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            if (width > 0)
                width = floorPOT(width);
            if (height > 0)
                height = floorPOT(height);
            break;
    }
}

void GridAtlasPacker::raiseToConstraint(int &width, int &height, DimensionsConstraint constraint) {
    if (squareConstraint(constraint))
        width = height = std::max(width, height);
    switch (constraint) {
        case DimensionsConstraint::NONE:
        case DimensionsConstraint::SQUARE:
            break;
        case DimensionsConstraint::EVEN_SQUARE:
            width += width&1;
            height += height&1;
            break;
        case DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE:
            width += -width&3;
            height += -height&3;
            break;
        case DimensionsConstraint::POWER_OF_TWO_RECTANGLE:
        case DimensionsConstraint::POWER_OF_TWO_SQUARE:
            if (width > 0)
                width = ceilPOT(width);
            if (height > 0)
                height = ceilPOT(height);
            break;
    }
}

double GridAtlasPacker::dimensionsRating(int width, int height, bool aligned) const {
    return ((double) width*width+(double) height*height)*(aligned ? 1-alignedColumnsBias : 1);
}

GridAtlasPacker::GridAtlasPacker() :
    columns(-1), rows(-1),
    width(-1), height(-1),
    cellWidth(-1), cellHeight(-1),
    padding(0),
    dimensionsConstraint(DimensionsConstraint::NONE),
    cellDimensionsConstraint(DimensionsConstraint::NONE),
    hFixed(false), vFixed(false),
    scale(-1),
    minScale(1),
    fixedX(0), fixedY(0),
    unitRange(0),
    pxRange(0),
    miterLimit(0),
    scaleMaximizationTolerance(.001),
    alignedColumnsBias(.125)
{ }

msdfgen::Shape::Bounds GridAtlasPacker::getMaxBounds(double &maxWidth, double &maxHeight, GlyphGeometry *glyphs, int count, double scale, double range) const {
    static const double LARGE_VALUE = 1e240;
    msdfgen::Shape::Bounds maxBounds = { +LARGE_VALUE, +LARGE_VALUE, -LARGE_VALUE, -LARGE_VALUE };
    for (GlyphGeometry *glyph = glyphs, *end = glyphs+count; glyph < end; ++glyph) {
        if (!glyph->isWhitespace()) {
            double geometryScale = glyph->getGeometryScale();
            double shapeRange = range/geometryScale;
            geometryScale *= scale;
            const msdfgen::Shape::Bounds &shapeBounds = glyph->getShapeBounds();
            double l = shapeBounds.l, b = shapeBounds.b, r = shapeBounds.r, t = shapeBounds.t;
            l -= .5*shapeRange, b -= .5*shapeRange;
            r += .5*shapeRange, t += .5*shapeRange;
            if (miterLimit > 0)
                glyph->getShape().boundMiters(l, b, r, t, .5*shapeRange, miterLimit, 1);
            l *= geometryScale, b *= geometryScale;
            r *= geometryScale, t *= geometryScale;
            maxBounds.l = std::min(maxBounds.l, l);
            maxBounds.b = std::min(maxBounds.b, b);
            maxBounds.r = std::max(maxBounds.r, r);
            maxBounds.t = std::max(maxBounds.t, t);
            maxWidth = std::max(maxWidth, r-l);
            maxHeight = std::max(maxHeight, t-b);
        }
    }
    if (maxBounds.l >= maxBounds.r || maxBounds.b >= maxBounds.t)
        maxBounds = msdfgen::Shape::Bounds();
    if (hFixed)
        maxWidth = maxBounds.r-maxBounds.l;
    if (vFixed)
        maxHeight = maxBounds.t-maxBounds.b;
    return maxBounds;
}

double GridAtlasPacker::scaleToFit(GlyphGeometry *glyphs, int count, int cellWidth, int cellHeight, msdfgen::Shape::Bounds &maxBounds, double &maxWidth, double &maxHeight) const {
    static const int BIG_VALUE = 1<<28;
    if (cellWidth <= 0)
        cellWidth = BIG_VALUE;
    if (cellHeight <= 0)
        cellHeight = BIG_VALUE;
    cellWidth -= padding, cellHeight -= padding;
    bool lastResult = false;
    #define TRY_FIT(scale) (maxWidth = 0, maxHeight = 0, maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, (scale), unitRange+pxRange/(scale)), lastResult = maxWidth <= cellWidth && maxHeight <= cellHeight)
    double minScale = 1, maxScale = 1;
    if (TRY_FIT(1)) {
        while (maxScale < 1e+32 && ((maxScale = 2*minScale), TRY_FIT(maxScale)))
            minScale = maxScale;
    } else {
        while (minScale > 1e-32 && ((minScale = .5*maxScale), !TRY_FIT(minScale)))
            maxScale = minScale;
    }
    if (minScale == maxScale)
        return 0;
    while (minScale/maxScale < 1-scaleMaximizationTolerance) {
        double midScale = .5*(minScale+maxScale);
        if (TRY_FIT(midScale))
            minScale = midScale;
        else
            maxScale = midScale;
    }
    if (!lastResult)
        TRY_FIT(minScale);
    return minScale;
}

// Ultra spaghetti
int GridAtlasPacker::pack(GlyphGeometry *glyphs, int count) {
    bool cellHeightFinal = cellHeight > 0;
    bool explicitRows = rows > 0;
    int cellCount = count;
    if (!cellCount)
        return 0;

    if (columns > 0 && rows > 0)
        cellCount = columns*rows;
    else if (columns > 0)
        rows = (cellCount+columns-1)/columns;
    else if (rows > 0)
        columns = (cellCount+rows-1)/rows;
    else if (width > 0 && cellWidth > 0) {
        columns = (width+padding)/cellWidth;
        rows = (cellCount+columns-1)/columns;
    }

    bool dimensionsChanged = false;
    if (width < 0 && cellWidth > 0 && columns > 0) {
        width = columns*cellWidth-padding;
        dimensionsChanged = true;
    }
    if (height < 0 && cellHeight > 0 && rows > 0) {
        height = rows*cellHeight-padding;
        dimensionsChanged = true;
    }
    if (dimensionsChanged)
        raiseToConstraint(width, height, dimensionsConstraint);

    dimensionsChanged = false;
    if (cellWidth < 0 && width > 0 && columns > 0) {
        cellWidth = (width+padding)/columns;
        dimensionsChanged = true;
    }
    if (cellHeight < 0 && height > 0 && rows > 0) {
        cellHeight = (height+padding)/rows;
        dimensionsChanged = true;
    }
    if (dimensionsChanged)
        lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);

    if ((cellWidth > 0 && cellWidth-padding <= pxRange) || (cellHeight > 0 && cellHeight-padding <= pxRange)) // cells definitely too small
        return -1;

    if (scale <= 0) {
        // If both pxRange and miterLimit is non-zero, miter bounds have to be computed for all potential scales
        if (pxRange && miterLimit > 0) {
            double maxWidth = 0, maxHeight = 0;
            msdfgen::Shape::Bounds maxBounds;

            if (cellWidth > 0 || cellHeight > 0) {
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
                if (scale < minScale)
                    return -1;
            }

            else if (width > 0 && height > 0) {
                double bestAlignedScale = 0;
                int bestCols = 0, bestAlignedCols = 0;
                for (int q = (int) sqrt(cellCount)+1; q > 0; --q) {
                    int cols = q;
                    int rows = (cellCount+cols-1)/cols;
                    int tWidth = (width+padding)/cols;
                    int tHeight = (height+padding)/rows;
                    if (!(tWidth > 0 && tHeight > 0))
                        continue;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    double curScale = scaleToFit(glyphs, count, tWidth, tHeight, maxBounds, maxWidth, maxHeight);
                    if (curScale > scale) {
                        scale = curScale;
                        bestCols = cols;
                    }
                    if (cols*tWidth == width && curScale > bestAlignedScale) {
                        bestAlignedScale = curScale;
                        bestAlignedCols = cols;
                    }

                    cols = (cellCount+q-1)/q;
                    rows = (cellCount+cols-1)/cols;
                    tWidth = (width+padding)/cols;
                    tHeight = (height+padding)/rows;
                    if (!(tWidth > 0 && tHeight > 0))
                        continue;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    curScale = scaleToFit(glyphs, count, tWidth, tHeight, maxBounds, maxWidth, maxHeight);
                    if (curScale > scale) {
                        scale = curScale;
                        bestCols = cols;
                    }
                    if (cols*tWidth == width && curScale > bestAlignedScale) {
                        bestAlignedScale = curScale;
                        bestAlignedCols = cols;
                    }
                }
                if (!bestCols)
                    return -1;
                // If columns can be aligned with total width at a slight cost to glyph scale, use that number of columns instead
                if (bestAlignedScale >= minScale && (alignedColumnsBias+1)*bestAlignedScale >= scale) {
                    scale = bestAlignedScale;
                    bestCols = bestAlignedCols;
                }

                columns = bestCols;
                rows = (cellCount+columns-1)/columns;
                cellWidth = (width+padding)/columns;
                cellHeight = (height+padding)/rows;
                lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
            }

            else {
                maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, minScale, unitRange+pxRange/minScale);
                cellWidth = (int) ceil(maxWidth)+padding;
                cellHeight = (int) ceil(maxHeight)+padding;
                raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                scale = scaleToFit(glyphs, count, cellWidth, cellHeight, maxBounds, maxWidth, maxHeight);
                if (scale < minScale)
                    maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, scale = minScale, unitRange+pxRange/minScale);
            }

            if (!explicitRows && !cellHeightFinal)
                cellHeight = (int) ceil(maxHeight)+padding;
            fixedX = (-maxBounds.l+.5*(cellWidth-padding-maxWidth))/scale;
            fixedY = (-maxBounds.b+.5*(cellHeight-padding-maxHeight))/scale;

        } else {

            double maxWidth = 0, maxHeight = 0;
            msdfgen::Shape::Bounds maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, 1, unitRange);

            double hScale = 0, vScale = 0;
            if (cellWidth > 0)
                hScale = (cellWidth-padding-pxRange)/maxWidth;
            if (cellHeight > 0)
                vScale = (cellHeight-padding-pxRange)/maxHeight;
            if (hScale || vScale) {
                if (hScale && vScale)
                    scale = std::min(hScale, vScale);
                else
                    scale = hScale+vScale;
                if (scale < minScale)
                    return -1;
            }

            else if (width > 0 && height > 0) {
                double bestAlignedScale = 0;
                int bestCols = 0, bestAlignedCols = 0;
                // TODO sqrtize
                for (int cols = 1; cols < width; ++cols) {
                    int rows = (cellCount+cols-1)/cols;
                    int tWidth = (width+padding)/cols;
                    int tHeight = (height+padding)/rows;
                    if (!(tWidth > 0 && tHeight > 0))
                        continue;
                    lowerToConstraint(tWidth, tHeight, cellDimensionsConstraint);
                    hScale = (tWidth-padding-pxRange)/maxWidth;
                    vScale = (tHeight-padding-pxRange)/maxHeight;
                    double curScale = std::min(hScale, vScale);
                    if (curScale > scale) {
                        scale = curScale;
                        bestCols = cols;
                    }
                    if (cols*tWidth == width && curScale > bestAlignedScale) {
                        bestAlignedScale = curScale;
                        bestAlignedCols = cols;
                    }
                }
                if (!bestCols)
                    return -1;
                // If columns can be aligned with total width at a slight cost to glyph scale, use that number of columns instead
                if (bestAlignedScale >= minScale && (alignedColumnsBias+1)*bestAlignedScale >= scale) {
                    scale = bestAlignedScale;
                    bestCols = bestAlignedCols;
                }

                columns = bestCols;
                rows = (cellCount+columns-1)/columns;
                cellWidth = (width+padding)/columns;
                cellHeight = (height+padding)/rows;
                lowerToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
            }

            else {
                cellWidth = (int) ceil(minScale*maxWidth+pxRange)+padding;
                cellHeight = (int) ceil(minScale*maxHeight+pxRange)+padding;
                raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
                hScale = (cellWidth-padding-pxRange)/maxWidth;
                vScale = (cellHeight-padding-pxRange)/maxHeight;
                scale = std::min(hScale, vScale);
            }

            if (!explicitRows && !cellHeightFinal)
                cellHeight = (int) ceil(scale*maxHeight+pxRange)+padding;
            fixedX = -maxBounds.l+.5*((cellWidth-padding)/scale-maxWidth);
            fixedY = -maxBounds.b+.5*((cellHeight-padding)/scale-maxHeight);

        }
    } else {
        double maxWidth = 0, maxHeight = 0;
        msdfgen::Shape::Bounds maxBounds = getMaxBounds(maxWidth, maxHeight, glyphs, count, scale, unitRange+pxRange/scale);

        if (cellWidth < 0 || cellHeight < 0) {
            cellWidth = (int) ceil(maxWidth)+padding;
            cellHeight = (int) ceil(maxHeight)+padding;
            raiseToConstraint(cellWidth, cellHeight, cellDimensionsConstraint);
        }

        fixedX = (-maxBounds.l+.5*(cellWidth-padding-maxWidth))/scale;
        fixedY = (-maxBounds.b+.5*(cellHeight-padding-maxHeight))/scale;
    }

    if (width < 0 || height < 0) {
        if (columns <= 0) {
            double bestRating = -1;
            for (int q = (int) sqrt(cellCount)+1; q > 0; --q) {
                int cols = q;
                int rows = (cellCount+cols-1)/cols;
                int curWidth = cols*cellWidth, curHeight = rows*cellHeight;
                raiseToConstraint(curWidth, curHeight, dimensionsConstraint);
                double rating = dimensionsRating(curWidth, curHeight, cols*cellWidth == curWidth);
                if (rating < bestRating || bestRating < 0) {
                    bestRating = rating;
                    columns = cols;
                }
                rows = q;
                cols = (cellCount+rows-1)/rows;
                curWidth = cols*cellWidth, curHeight = rows*cellHeight;
                raiseToConstraint(curWidth, curHeight, dimensionsConstraint);
                rating = dimensionsRating(curWidth, curHeight, cols*cellWidth == curWidth);
                if (rating < bestRating || bestRating < 0) {
                    bestRating = rating;
                    columns = cols;
                }
            }
            rows = (cellCount+columns-1)/columns;
        }
        width = columns*cellWidth, height = rows*cellHeight;
        raiseToConstraint(width, height, dimensionsConstraint);
    }

    if (columns < 0) {
        columns = (width+padding)/cellWidth;
        rows = (cellCount+columns-1)/columns;
    }

    int col = 0, row = 0;
    for (GlyphGeometry *glyph = glyphs, *end = glyphs+count; glyph < end; ++glyph) {
        if (!glyph->isWhitespace()) {
            Rectangle rect = { };
            glyph->frameBox(scale, unitRange+pxRange/scale, miterLimit, cellWidth-padding, cellHeight-padding, hFixed ? &fixedX : nullptr, vFixed ? &fixedY : nullptr);
            glyph->placeBox(col*cellWidth, height-(row+1)*cellHeight);
            if (++col >= columns) {
                if (++row >= rows) {
                    return end-glyph-1;
                }
                col = 0;
            }
        }
    }

    if (columns*rows < cellCount) {
        // TODO return lower number
    }
    return 0;
}

void GridAtlasPacker::setFixedOrigin(bool horizontal, bool vertical) {
    hFixed = horizontal, vFixed = vertical;
}

void GridAtlasPacker::setCellDimensions(int width, int height) {
    cellWidth = width, cellHeight = height;
}

void GridAtlasPacker::unsetCellDimensions() {
    cellWidth = -1, cellHeight = -1;
}

void GridAtlasPacker::setCellDimensionsConstraint(DimensionsConstraint dimensionsConstraint) {
    cellDimensionsConstraint = dimensionsConstraint;
}

void GridAtlasPacker::setColumns(int columns) {
    this->columns = columns;
}

void GridAtlasPacker::setRows(int rows) {
    this->rows = rows;
}

void GridAtlasPacker::unsetColumns() {
    columns = -1;
}

void GridAtlasPacker::unsetRows() {
    rows = -1;
}

void GridAtlasPacker::setDimensions(int width, int height) {
    this->width = width, this->height = height;
}

void GridAtlasPacker::unsetDimensions() {
    width = -1, height = -1;
}

void GridAtlasPacker::setDimensionsConstraint(DimensionsConstraint dimensionsConstraint) {
    this->dimensionsConstraint = dimensionsConstraint;
}

void GridAtlasPacker::setPadding(int padding) {
    this->padding = padding;
}

void GridAtlasPacker::setScale(double scale) {
    this->scale = scale;
}

void GridAtlasPacker::setMinimumScale(double minScale) {
    this->minScale = minScale;
}

void GridAtlasPacker::setUnitRange(double unitRange) {
    this->unitRange = unitRange;
}

void GridAtlasPacker::setPixelRange(double pxRange) {
    this->pxRange = pxRange;
}

void GridAtlasPacker::setMiterLimit(double miterLimit) {
    this->miterLimit = miterLimit;
}

void GridAtlasPacker::getDimensions(int &width, int &height) const {
    width = this->width, height = this->height;
}

void GridAtlasPacker::getCellDimensions(int &width, int &height) const {
    width = cellWidth, height = cellHeight;
}

int GridAtlasPacker::getColumns() const {
    return columns;
}

int GridAtlasPacker::getRows() const {
    return rows;
}

double GridAtlasPacker::getScale() const {
    return scale;
}

double GridAtlasPacker::getPixelRange() const {
    return pxRange;
}

void GridAtlasPacker::getFixedOrigin(double &x, double &y) {
    x = fixedX-.5/scale;
    y = fixedY-.5/scale;
}

}
