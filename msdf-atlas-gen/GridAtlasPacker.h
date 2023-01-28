
#pragma once

#include "GlyphGeometry.h"

namespace msdf_atlas {

/**
 * This class computes the layout of a static uniform grid atlas and may optionally
 * also find the minimum required dimensions and/or the maximum glyph scale
 */
class GridAtlasPacker {

public:
    GridAtlasPacker();

    /// Computes the layout for the array of glyphs. Returns 0 on success
    int pack(GlyphGeometry *glyphs, int count);

    /// Sets whether the origin point should be at the same position in each glyph, separately for horizontal and vertical dimension
    void setFixedOrigin(bool horizontal, bool vertical);
    void setCellDimensions(int width, int height);
    void unsetCellDimensions();
    void setCellDimensionsConstraint(DimensionsConstraint dimensionsConstraint);
    void setColumns(int columns);
    void setRows(int rows);
    void unsetColumns();
    void unsetRows();

    /// Sets the atlas's fixed dimensions
    void setDimensions(int width, int height);
    /// Sets the atlas's dimensions to be determined during pack
    void unsetDimensions();
    /// Sets the constraint to be used when determining dimensions
    void setDimensionsConstraint(DimensionsConstraint dimensionsConstraint);
    /// Sets the padding between glyph boxes
    void setPadding(int padding);
    /// Sets fixed glyph scale
    void setScale(double scale);
    /// Sets the minimum glyph scale
    void setMinimumScale(double minScale);
    /// Sets the unit component of the total distance range
    void setUnitRange(double unitRange);
    /// Sets the pixel component of the total distance range
    void setPixelRange(double pxRange);
    /// Sets the miter limit for bounds computation
    void setMiterLimit(double miterLimit);

    /// Outputs the atlas's final dimensions
    void getDimensions(int &width, int &height) const;
    void getCellDimensions(int &width, int &height) const;
    int getColumns() const;
    int getRows() const;
    /// Returns the final glyph scale
    double getScale() const;
    /// Returns the final combined pixel range (including converted unit range)
    double getPixelRange() const;
    void getFixedOrigin(double &x, double &y);

private:
    int columns, rows;
    int width, height;
    int cellWidth, cellHeight;
    int padding;
    DimensionsConstraint dimensionsConstraint;
    DimensionsConstraint cellDimensionsConstraint;
    bool hFixed, vFixed;
    double scale;
    double minScale;
    double fixedX, fixedY;
    double unitRange;
    double pxRange;
    double miterLimit;
    double scaleMaximizationTolerance;
    double alignedColumnsBias;

    static void lowerToConstraint(int &width, int &height, DimensionsConstraint constraint);
    static void raiseToConstraint(int &width, int &height, DimensionsConstraint constraint);

    double dimensionsRating(int width, int height, bool aligned) const;
    msdfgen::Shape::Bounds getMaxBounds(double &maxWidth, double &maxHeight, GlyphGeometry *glyphs, int count, double scale, double range) const;
    double scaleToFit(GlyphGeometry *glyphs, int count, int cellWidth, int cellHeight, msdfgen::Shape::Bounds &maxBounds, double &maxWidth, double &maxHeight) const;

};

}
