/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../Paint.h"

#include "../../Context.h"
#include "../../Game.h"
#include "../../config/Config.h"
#include "../../core/Numerics.hpp"
#include "../../drawing/LightFX.h"
#include "../../entity/EntityRegistry.h"
#include "../../entity/Peep.h"
#include "../../entity/Staff.h"
#include "../../interface/Viewport.h"
#include "../../localisation/Formatter.h"
#include "../../localisation/Localisation.h"
#include "../../object/FootpathObject.h"
#include "../../object/FootpathRailingsObject.h"
#include "../../object/FootpathSurfaceObject.h"
#include "../../object/ObjectList.h"
#include "../../object/ObjectManager.h"
#include "../../ride/Ride.h"
#include "../../ride/Track.h"
#include "../../ride/TrackDesign.h"
#include "../../ride/TrackPaint.h"
#include "../../world/Footpath.h"
#include "../../world/Map.h"
#include "../../world/Scenery.h"
#include "../../world/Surface.h"
#include "../../world/TileInspector.h"
#include "../Supports.h"
#include "Paint.Surface.h"
#include "Paint.TileElement.h"

using namespace OpenRCT2;

bool gPaintWidePathsAsGhost = false;

const uint8_t PathSlopeToLandSlope[] = {
    TILE_ELEMENT_SLOPE_SW_SIDE_UP,
    TILE_ELEMENT_SLOPE_NW_SIDE_UP,
    TILE_ELEMENT_SLOPE_NE_SIDE_UP,
    TILE_ELEMENT_SLOPE_SE_SIDE_UP,
};

static constexpr const uint8_t byte_98D6E0[] = {
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 12, 13, 14, 15, 0, 1, 2, 20, 4, 5, 6, 22, 8, 9, 10, 26, 12, 13, 14, 36,
    0, 1, 2, 3, 4, 5, 21, 23, 8, 9,  10, 11, 12, 13, 33, 37, 0, 1, 2, 3,  4, 5, 6, 24, 8, 9, 10, 11, 12, 13, 14, 38,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 29, 30, 34, 39, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 40,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 12, 13, 35, 41, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 42,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 25, 10, 27, 12, 31, 14, 43, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 28, 12, 13, 14, 44,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 12, 13, 14, 45, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 46,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 12, 32, 14, 47, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 48,
    0, 1, 2, 3, 4, 5, 6,  7,  8, 9,  10, 11, 12, 13, 14, 49, 0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 50,
};

// clang-format off
static constexpr const int16_t stru_98D804[][4] = {
    { 3, 3, 26, 26 },
    { 0, 3, 29, 26 },
    { 3, 3, 26, 29 },
    { 0, 3, 29, 29 },
    { 3, 3, 29, 26 },
    { 0, 3, 32, 26 },
    { 3, 3, 29, 29 },
    { 0, 3, 32, 29 },
    { 3, 0, 26, 29 },
    { 0, 0, 29, 29 },
    { 3, 0, 26, 32 },
    { 0, 0, 29, 32 },
    { 3, 0, 29, 29 },
    { 0, 0, 32, 29 },
    { 3, 0, 29, 32 },
    { 0, 0, 32, 32 },
};

static constexpr const uint8_t byte_98D8A4[] = {
    0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0
};
// clang-format on

void path_paint_box_support(
    paint_session& session, const PathElement& pathElement, int32_t height, const FootpathPaintInfo& pathPaintInfo,
    bool hasSupports, ImageId imageTemplate, ImageId sceneryImageTemplate);
void path_paint_pole_support(
    paint_session& session, const PathElement& pathElement, int16_t height, const FootpathPaintInfo& pathPaintInfo,
    bool hasSupports, ImageId imageTemplate, ImageId sceneryImageTemplate);

static ImageIndex GetEdgeImageOffset(edge_t edge)
{
    switch (edge)
    {
        case EDGE_NE:
            return 1;
        case EDGE_SE:
            return 2;
        case EDGE_SW:
            return 3;
        case EDGE_NW:
            return 4;
        default:
            return 0;
    }
}

static ImageIndex GetFootpathLampImage(const PathBitEntry& pathBitEntry, edge_t edge, bool isBroken)
{
    auto offset = GetEdgeImageOffset(edge);
    if (offset == 0)
        return ImageIndexUndefined;
    return pathBitEntry.image + offset + (isBroken ? 4 : 0);
}

static ImageIndex GetFootpathBinImage(const PathBitEntry& pathBitEntry, edge_t edge, bool isBroken, bool isFull)
{
    auto offset = GetEdgeImageOffset(edge);
    if (offset == 0)
        return ImageIndexUndefined;

    auto stateOffset = isBroken ? 4 : (isFull ? 8 : 0);
    return pathBitEntry.image + offset + stateOffset;
}

static ImageIndex GetFootpathBenchImage(const PathBitEntry& pathBitEntry, edge_t edge, bool isBroken)
{
    auto offset = GetEdgeImageOffset(edge);
    if (offset == 0)
        return ImageIndexUndefined;
    return pathBitEntry.image + offset + (isBroken ? 4 : 0);
}

/* rct2: 0x006A5AE5 */
static void path_bit_lights_paint(
    paint_session& session, const PathBitEntry& pathBitEntry, const PathElement& pathElement, int32_t height, uint8_t edges,
    ImageId imageTemplate)
{
    if (pathElement.IsSloped())
        height += 8;

    auto isBroken = pathElement.IsBroken();
    if (!(edges & EDGE_NE))
    {
        auto imageIndex = GetFootpathLampImage(pathBitEntry, EDGE_NE, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 2, 16, height }, { 1, 1, 23 }, { 3, 16, height + 2 });
    }
    if (!(edges & EDGE_SE))
    {
        auto imageIndex = GetFootpathLampImage(pathBitEntry, EDGE_SE, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 16, 30, height }, { 1, 0, 23 }, { 16, 29, height + 2 });
    }
    if (!(edges & EDGE_SW))
    {
        auto imageIndex = GetFootpathLampImage(pathBitEntry, EDGE_SW, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 30, 16, height }, { 0, 1, 23 }, { 29, 16, height + 2 });
    }
    if (!(edges & EDGE_NW))
    {
        auto imageIndex = GetFootpathLampImage(pathBitEntry, EDGE_NW, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 16, 2, height }, { 1, 1, 23 }, { 16, 3, height + 2 });
    }
}

static bool IsBinFull(paint_session& session, const PathElement& pathElement, edge_t edge)
{
    switch (edge)
    {
        case EDGE_NE:
            return !(pathElement.GetAdditionStatus() & Numerics::ror8(0x03, (2 * session.CurrentRotation)));
        case EDGE_SE:
            return !(pathElement.GetAdditionStatus() & Numerics::ror8(0x0C, (2 * session.CurrentRotation)));
        case EDGE_SW:
            return !(pathElement.GetAdditionStatus() & Numerics::ror8(0x30, (2 * session.CurrentRotation)));
        case EDGE_NW:
            return !(pathElement.GetAdditionStatus() & Numerics::ror8(0xC0, (2 * session.CurrentRotation)));
        default:
            return false;
    }
}

/* rct2: 0x006A5C94 */
static void path_bit_bins_paint(
    paint_session& session, const PathBitEntry& pathBitEntry, const PathElement& pathElement, int32_t height, uint8_t edges,
    ImageId imageTemplate)
{
    if (pathElement.IsSloped())
        height += 8;

    bool binsAreVandalised = pathElement.IsBroken();
    auto highlightPathIssues = (session.ViewFlags & VIEWPORT_FLAG_HIGHLIGHT_PATH_ISSUES) != 0;

    if (!(edges & EDGE_NE))
    {
        auto binIsFull = IsBinFull(session, pathElement, EDGE_NE);
        auto imageIndex = GetFootpathBinImage(pathBitEntry, EDGE_NE, binsAreVandalised, binIsFull);
        if (!highlightPathIssues || binIsFull || binsAreVandalised)
            PaintAddImageAsParent(
                session, imageTemplate.WithIndex(imageIndex), { 7, 16, height }, { 1, 1, 7 }, { 7, 16, height + 2 });
    }
    if (!(edges & EDGE_SE))
    {
        auto binIsFull = IsBinFull(session, pathElement, EDGE_SE);
        auto imageIndex = GetFootpathBinImage(pathBitEntry, EDGE_SE, binsAreVandalised, binIsFull);
        if (!highlightPathIssues || binIsFull || binsAreVandalised)
            PaintAddImageAsParent(
                session, imageTemplate.WithIndex(imageIndex), { 16, 25, height }, { 1, 1, 7 }, { 16, 25, height + 2 });
    }

    if (!(edges & EDGE_SW))
    {
        auto binIsFull = IsBinFull(session, pathElement, EDGE_SW);
        auto imageIndex = GetFootpathBinImage(pathBitEntry, EDGE_SW, binsAreVandalised, binIsFull);
        if (!highlightPathIssues || binIsFull || binsAreVandalised)
            PaintAddImageAsParent(
                session, imageTemplate.WithIndex(imageIndex), { 25, 16, height }, { 1, 1, 7 }, { 25, 16, height + 2 });
    }

    if (!(edges & EDGE_NW))
    {
        auto binIsFull = IsBinFull(session, pathElement, EDGE_NW);
        auto imageIndex = GetFootpathBinImage(pathBitEntry, EDGE_NW, binsAreVandalised, binIsFull);
        if (!highlightPathIssues || binIsFull || binsAreVandalised)
            PaintAddImageAsParent(
                session, imageTemplate.WithIndex(imageIndex), { 16, 7, height }, { 1, 1, 7 }, { 16, 7, height + 2 });
    }
}

/* rct2: 0x006A5E81 */
static void path_bit_benches_paint(
    paint_session& session, const PathBitEntry& pathBitEntry, const PathElement& pathElement, int32_t height, uint8_t edges,
    ImageId imageTemplate)
{
    auto isBroken = pathElement.IsBroken();
    if (!(edges & EDGE_NE))
    {
        auto imageIndex = GetFootpathBenchImage(pathBitEntry, EDGE_NE, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 7, 16, height }, { 0, 16, 7 }, { 6, 8, height + 2 });
    }
    if (!(edges & EDGE_SE))
    {
        auto imageIndex = GetFootpathBenchImage(pathBitEntry, EDGE_SE, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 16, 25, height }, { 16, 0, 7 }, { 8, 23, height + 2 });
    }

    if (!(edges & EDGE_SW))
    {
        auto imageIndex = GetFootpathBenchImage(pathBitEntry, EDGE_SW, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 25, 16, height }, { 0, 16, 7 }, { 23, 8, height + 2 });
    }

    if (!(edges & EDGE_NW))
    {
        auto imageIndex = GetFootpathBenchImage(pathBitEntry, EDGE_NW, isBroken);
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(imageIndex), { 16, 7, height }, { 16, 0, 7 }, { 8, 6, height + 2 });
    }
}

/* rct2: 0x006A6008 */
static void path_bit_jumping_fountains_paint(
    paint_session& session, const PathBitEntry& pathBitEntry, int32_t height, ImageId imageTemplate, rct_drawpixelinfo* dpi)
{
    if (dpi->zoom_level > ZoomLevel{ 0 })
        return;

    auto imageId = imageTemplate.WithIndex(pathBitEntry.image);
    PaintAddImageAsParent(session, imageId.WithIndexOffset(1), { 0, 0, height }, { 1, 1, 2 }, { 3, 3, height + 2 });
    PaintAddImageAsParent(session, imageId.WithIndexOffset(2), { 0, 0, height }, { 1, 1, 2 }, { 3, 29, height + 2 });
    PaintAddImageAsParent(session, imageId.WithIndexOffset(3), { 0, 0, height }, { 1, 1, 2 }, { 29, 29, height + 2 });
    PaintAddImageAsParent(session, imageId.WithIndexOffset(4), { 0, 0, height }, { 1, 1, 2 }, { 29, 3, height + 2 });
}

/**
 * rct2: 0x006A4101
 * @param tile_element (esi)
 */
static void sub_6A4101(
    paint_session& session, const PathElement& pathElement, uint16_t height, uint32_t connectedEdges, bool hasSupports,
    const FootpathPaintInfo& pathPaintInfo, ImageId imageTemplate)
{
    auto imageId = imageTemplate.WithIndex(pathPaintInfo.RailingsImageId);
    if (pathElement.IsQueue())
    {
        uint8_t local_ebp = connectedEdges & 0x0F;
        if (pathElement.IsSloped())
        {
            switch ((pathElement.GetSlopeDirection() + session.CurrentRotation) & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK)
            {
                case 0:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(22), { 0, 4, height }, { 32, 1, 23 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(22), { 0, 28, height }, { 32, 1, 23 }, { 0, 28, height + 2 });
                    break;
                case 1:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(21), { 4, 0, height }, { 1, 32, 23 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(21), { 28, 0, height }, { 1, 32, 23 }, { 28, 0, height + 2 });
                    break;
                case 2:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(23), { 0, 4, height }, { 32, 1, 23 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(23), { 0, 28, height }, { 32, 1, 23 }, { 0, 28, height + 2 });
                    break;
                case 3:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(20), { 4, 0, height }, { 1, 32, 23 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(20), { 28, 0, height }, { 1, 32, 23 }, { 28, 0, height + 2 });
                    break;
            }
        }
        else
        {
            switch (local_ebp)
            {
                case 1:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(17), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(17), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                    break;
                case 2:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(18), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(18), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                    break;
                case 3:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(17), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(18), { 28, 0, height }, { 1, 28, 7 },
                        { 28, 4, height + 2 }); // bound_box_offset_y seems to be a bug
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(25), { 0, 0, height }, { 4, 4, 7 }, { 0, 28, height + 2 });
                    break;
                case 4:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(19), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(19), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                    break;
                case 5:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(15), { 0, 4, height }, { 32, 1, 7 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(15), { 0, 28, height }, { 32, 1, 7 }, { 0, 28, height + 2 });
                    break;
                case 6:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(18), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(19), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(26), { 0, 0, height }, { 4, 4, 7 }, { 28, 28, height + 2 });
                    break;
                case 8:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(16), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(16), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                    break;
                case 9:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(16), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(17), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(24), { 0, 0, height }, { 4, 4, 7 }, { 0, 0, height + 2 });
                    break;
                case 10:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(14), { 4, 0, height }, { 1, 32, 7 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(14), { 28, 0, height }, { 1, 32, 7 }, { 28, 0, height + 2 });
                    break;
                case 12:
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(16), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(19), { 0, 28, height }, { 28, 1, 7 },
                        { 4, 28, height + 2 }); // bound_box_offset_x seems to be a bug
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(27), { 0, 0, height }, { 4, 4, 7 }, { 28, 0, height + 2 });
                    break;
                default:
                    // purposely left empty
                    break;
            }
        }

        if (!pathElement.HasQueueBanner() || (pathPaintInfo.RailingFlags & RAILING_ENTRY_FLAG_NO_QUEUE_BANNER))
        {
            return;
        }

        uint8_t direction = pathElement.GetQueueBannerDirection();
        // Draw ride sign
        session.InteractionType = ViewportInteractionItem::Ride;
        if (pathElement.IsSloped())
        {
            if (pathElement.GetSlopeDirection() == direction)
                height += 16;
        }
        direction += session.CurrentRotation;
        direction &= 3;

        CoordsXYZ boundBoxOffsets = CoordsXYZ(BannerBoundBoxes[direction][0], height + 2);

        imageId = imageId.WithIndexOffset(28 + (direction << 1));

        // Draw pole in the back
        PaintAddImageAsParent(session, imageId, { 0, 0, height }, { 1, 1, 21 }, boundBoxOffsets);

        // Draw pole in the front and banner
        boundBoxOffsets.x = BannerBoundBoxes[direction][1].x;
        boundBoxOffsets.y = BannerBoundBoxes[direction][1].y;
        imageId = imageId.WithIndexOffset(1);
        PaintAddImageAsParent(session, imageId, { 0, 0, height }, { 1, 1, 21 }, boundBoxOffsets);

        direction--;
        // If text shown
        auto ride = get_ride(pathElement.GetRideIndex());
        if (direction < 2 && ride != nullptr && !imageTemplate.IsRemap())
        {
            uint16_t scrollingMode = pathPaintInfo.ScrollingMode;
            scrollingMode += direction;

            auto ft = Formatter();

            if (ride->status == RideStatus::Open && !(ride->lifecycle_flags & RIDE_LIFECYCLE_BROKEN_DOWN))
            {
                ft.Add<rct_string_id>(STR_RIDE_ENTRANCE_NAME);
                ride->FormatNameTo(ft);
            }
            else
            {
                ft.Add<rct_string_id>(STR_RIDE_ENTRANCE_CLOSED);
            }
            if (gConfigGeneral.upper_case_banners)
            {
                format_string_to_upper(
                    gCommonStringFormatBuffer, sizeof(gCommonStringFormatBuffer), STR_BANNER_TEXT_FORMAT, ft.Data());
            }
            else
            {
                format_string(gCommonStringFormatBuffer, sizeof(gCommonStringFormatBuffer), STR_BANNER_TEXT_FORMAT, ft.Data());
            }

            uint16_t stringWidth = gfx_get_string_width(gCommonStringFormatBuffer, FontSpriteBase::TINY);
            uint16_t scroll = stringWidth > 0 ? (gCurrentTicks / 2) % stringWidth : 0;

            PaintAddImageAsChild(
                session, scrolling_text_setup(session, STR_BANNER_TEXT_FORMAT, ft, scroll, scrollingMode, COLOUR_BLACK), 0, 0,
                1, 1, 21, height + 7, boundBoxOffsets.x, boundBoxOffsets.y, boundBoxOffsets.z);
        }

        session.InteractionType = ViewportInteractionItem::Footpath;
        if (imageTemplate.IsRemap())
        {
            session.InteractionType = ViewportInteractionItem::None;
        }
        return;
    }

    uint32_t drawnCorners = 0;
    // If the path is not drawn over the supports, then no corner sprites will be drawn (making double-width paths
    // look like connected series of intersections).
    if (pathPaintInfo.RailingFlags & RAILING_ENTRY_FLAG_DRAW_PATH_OVER_SUPPORTS)
    {
        drawnCorners = (connectedEdges & FOOTPATH_PROPERTIES_EDGES_CORNERS_MASK) >> 4;
    }

    auto slopeRailingsSupported = !(pathPaintInfo.SurfaceFlags & FOOTPATH_ENTRY_FLAG_NO_SLOPE_RAILINGS);
    if ((hasSupports || slopeRailingsSupported) && pathElement.IsSloped())
    {
        switch ((pathElement.GetSlopeDirection() + session.CurrentRotation) & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK)
        {
            case 0:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(8), { 0, 4, height }, { 32, 1, 23 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(8), { 0, 28, height }, { 32, 1, 23 }, { 0, 28, height + 2 });
                break;
            case 1:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(7), { 4, 0, height }, { 1, 32, 23 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(7), { 28, 0, height }, { 1, 32, 23 }, { 28, 0, height + 2 });
                break;
            case 2:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(9), { 0, 4, height }, { 32, 1, 23 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(9), { 0, 28, height }, { 32, 1, 23 }, { 0, 28, height + 2 });
                break;
            case 3:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(6), { 4, 0, height }, { 1, 32, 23 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(6), { 28, 0, height }, { 1, 32, 23 }, { 28, 0, height + 2 });
                break;
        }
    }
    else
    {
        if (!hasSupports)
        {
            return;
        }

        switch (connectedEdges & FOOTPATH_PROPERTIES_EDGES_EDGES_MASK)
        {
            case 0:
                // purposely left empty
                break;
            case 1:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(3), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(3), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                break;
            case 2:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(4), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(4), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                break;
            case 4:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(5), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(5), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                break;
            case 5:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(1), { 0, 4, height }, { 32, 1, 7 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(1), { 0, 28, height }, { 32, 1, 7 }, { 0, 28, height + 2 });
                break;
            case 8:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(2), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(2), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                break;
            case 10:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(0), { 4, 0, height }, { 1, 32, 7 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(0), { 28, 0, height }, { 1, 32, 7 }, { 28, 0, height + 2 });
                break;

            case 3:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(3), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(4), { 28, 0, height }, { 1, 28, 7 },
                    { 28, 4, height + 2 }); // bound_box_offset_y seems to be a bug
                if (!(drawnCorners & FOOTPATH_CORNER_0))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(11), { 0, 0, height }, { 4, 4, 7 }, { 0, 28, height + 2 });
                }
                break;
            case 6:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(4), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(5), { 0, 4, height }, { 28, 1, 7 }, { 0, 4, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_1))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(12), { 0, 0, height }, { 4, 4, 7 }, { 28, 28, height + 2 });
                }
                break;
            case 9:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(2), { 28, 0, height }, { 1, 28, 7 }, { 28, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(3), { 0, 28, height }, { 28, 1, 7 }, { 0, 28, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_3))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(10), { 0, 0, height }, { 4, 4, 7 }, { 0, 0, height + 2 });
                }
                break;
            case 12:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(2), { 4, 0, height }, { 1, 28, 7 }, { 4, 0, height + 2 });
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(5), { 0, 28, height }, { 28, 1, 7 },
                    { 4, 28, height + 2 }); // bound_box_offset_x seems to be a bug
                if (!(drawnCorners & FOOTPATH_CORNER_2))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(13), { 0, 0, height }, { 4, 4, 7 }, { 28, 0, height + 2 });
                }
                break;

            case 7:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(1), { 0, 4, height }, { 32, 1, 7 }, { 0, 4, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_0))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(11), { 0, 0, height }, { 4, 4, 7 }, { 0, 28, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_1))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(12), { 0, 0, height }, { 4, 4, 7 }, { 28, 28, height + 2 });
                }
                break;
            case 13:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(1), { 0, 28, height }, { 32, 1, 7 }, { 0, 28, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_2))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(13), { 0, 0, height }, { 4, 4, 7 }, { 28, 0, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_3))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(10), { 0, 0, height }, { 4, 4, 7 }, { 0, 0, height + 2 });
                }
                break;
            case 14:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(0), { 4, 0, height }, { 1, 32, 7 }, { 4, 0, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_1))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(12), { 0, 0, height }, { 4, 4, 7 }, { 28, 28, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_2))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(13), { 0, 0, height }, { 4, 4, 7 }, { 28, 0, height + 2 });
                }
                break;
            case 11:
                PaintAddImageAsParent(
                    session, imageId.WithIndexOffset(0), { 28, 0, height }, { 1, 32, 7 }, { 28, 0, height + 2 });
                if (!(drawnCorners & FOOTPATH_CORNER_0))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(11), { 0, 0, height }, { 4, 4, 7 }, { 0, 28, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_3))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(10), { 0, 0, height }, { 4, 4, 7 }, { 0, 0, height + 2 });
                }
                break;

            case 15:
                if (!(drawnCorners & FOOTPATH_CORNER_0))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(11), { 0, 0, height }, { 4, 4, 7 }, { 0, 28, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_1))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(12), { 0, 0, height }, { 4, 4, 7 }, { 28, 28, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_2))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(13), { 0, 0, height }, { 4, 4, 7 }, { 28, 0, height + 2 });
                }
                if (!(drawnCorners & FOOTPATH_CORNER_3))
                {
                    PaintAddImageAsParent(
                        session, imageId.WithIndexOffset(10), { 0, 0, height }, { 4, 4, 7 }, { 0, 0, height + 2 });
                }
                break;
        }
    }
}

/**
 * rct2: 0x006A3F61
 * @param pathElement (esp[0])
 * @param connectedEdges (bp) (relative to the camera's rotation)
 * @param height (dx)
 * @param pathPaintInfo (0x00F3EF6C)
 * @param imageFlags (0x00F3EF70)
 * @param sceneryImageFlags (0x00F3EF74)
 */
static void sub_6A3F61(
    paint_session& session, const PathElement& pathElement, uint16_t connectedEdges, uint16_t height,
    const FootpathPaintInfo& pathPaintInfo, ImageId imageTemplate, ImageId sceneryImageTemplate, bool hasSupports)
{
    // eax --
    // ebx --
    // ecx
    // edx
    // esi --
    // edi --
    // ebp
    // esp: [ esi, ???, 000]

    // Probably drawing benches etc.

    rct_drawpixelinfo* dpi = &session.DPI;

    if (dpi->zoom_level <= ZoomLevel{ 1 })
    {
        bool paintScenery = true;

        if (!gTrackDesignSaveMode)
        {
            if (pathElement.HasAddition())
            {
                session.InteractionType = ViewportInteractionItem::FootpathItem;
                if (sceneryImageTemplate.IsRemap())
                {
                    session.InteractionType = ViewportInteractionItem::None;
                }

                // Draw additional path bits (bins, benches, lamps, queue screens)
                auto* pathAddEntry = pathElement.GetAdditionEntry();

                // Can be null if the object is not loaded.
                if (pathAddEntry == nullptr)
                {
                    paintScenery = false;
                }
                else if (
                    (session.ViewFlags & VIEWPORT_FLAG_HIGHLIGHT_PATH_ISSUES) && !(pathElement.IsBroken())
                    && pathAddEntry->draw_type != PathBitDrawType::Bin)
                {
                    paintScenery = false;
                }
                else
                {
                    switch (pathAddEntry->draw_type)
                    {
                        case PathBitDrawType::Light:
                            path_bit_lights_paint(
                                session, *pathAddEntry, pathElement, height, static_cast<uint8_t>(connectedEdges),
                                sceneryImageTemplate);
                            break;
                        case PathBitDrawType::Bin:
                            path_bit_bins_paint(
                                session, *pathAddEntry, pathElement, height, static_cast<uint8_t>(connectedEdges),
                                sceneryImageTemplate);
                            break;
                        case PathBitDrawType::Bench:
                            path_bit_benches_paint(
                                session, *pathAddEntry, pathElement, height, static_cast<uint8_t>(connectedEdges),
                                sceneryImageTemplate);
                            break;
                        case PathBitDrawType::JumpingFountain:
                            path_bit_jumping_fountains_paint(session, *pathAddEntry, height, sceneryImageTemplate, dpi);
                            break;
                    }

                    session.InteractionType = ViewportInteractionItem::Footpath;

                    if (sceneryImageTemplate.IsRemap())
                    {
                        session.InteractionType = ViewportInteractionItem::None;
                    }
                }
            }
        }

        // Redundant zoom-level check removed

        if (paintScenery)
            sub_6A4101(session, pathElement, height, connectedEdges, hasSupports, pathPaintInfo, imageTemplate);
    }

    // This is about tunnel drawing
    uint8_t direction = (pathElement.GetSlopeDirection() + session.CurrentRotation) & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK;
    bool sloped = pathElement.IsSloped();

    if (connectedEdges & EDGE_SE)
    {
        // Bottom right of tile is a tunnel
        if (sloped && direction == EDGE_NE)
        {
            // Path going down into the tunnel
            paint_util_push_tunnel_right(session, height + 16, TUNNEL_PATH_AND_MINI_GOLF);
        }
        else if (connectedEdges & EDGE_NE)
        {
            // Flat path with edge to the right (north-east)
            paint_util_push_tunnel_right(session, height, TUNNEL_PATH_11);
        }
        else
        {
            // Path going up, or flat and not connected to the right
            paint_util_push_tunnel_right(session, height, TUNNEL_PATH_AND_MINI_GOLF);
        }
    }

    if (!(connectedEdges & EDGE_SW))
    {
        return;
    }

    // Bottom left of the tile is a tunnel
    if (sloped && direction == EDGE_SE)
    {
        // Path going down into the tunnel
        paint_util_push_tunnel_left(session, height + 16, TUNNEL_PATH_AND_MINI_GOLF);
    }
    else if (connectedEdges & EDGE_NW)
    {
        // Flat path with edge to the left (north-west)
        paint_util_push_tunnel_left(session, height, TUNNEL_PATH_11);
    }
    else
    {
        // Path going up, or flat and not connected to the left
        paint_util_push_tunnel_left(session, height, TUNNEL_PATH_AND_MINI_GOLF);
    }
}

static FootpathPaintInfo GetFootpathPaintInfo(const PathElement& pathEl)
{
    FootpathPaintInfo pathPaintInfo;

    const auto* surfaceDescriptor = pathEl.GetSurfaceDescriptor();
    if (surfaceDescriptor != nullptr)
    {
        pathPaintInfo.SurfaceImageId = surfaceDescriptor->Image;
        pathPaintInfo.SurfaceFlags = surfaceDescriptor->Flags;
    }

    const auto* railingsDescriptor = pathEl.GetRailingsDescriptor();
    if (railingsDescriptor != nullptr)
    {
        pathPaintInfo.BridgeImageId = railingsDescriptor->BridgeImage;
        pathPaintInfo.RailingsImageId = railingsDescriptor->RailingsImage;
        pathPaintInfo.RailingFlags = railingsDescriptor->Flags;
        pathPaintInfo.ScrollingMode = railingsDescriptor->ScrollingMode;
        pathPaintInfo.SupportType = railingsDescriptor->SupportType;
        pathPaintInfo.SupportColour = railingsDescriptor->SupportColour;
    }

    return pathPaintInfo;
}

static bool ShouldDrawSupports(paint_session& session, const PathElement& pathEl, uint16_t height)
{
    auto surface = map_get_surface_element_at(session.MapPosition);
    if (surface == nullptr)
    {
        return true;
    }
    else if (surface->GetBaseZ() != height)
    {
        const auto* surfaceEntry = pathEl.GetSurfaceEntry();
        const bool showUndergroundRailings = surfaceEntry == nullptr
            || !(surfaceEntry->Flags & FOOTPATH_ENTRY_FLAG_NO_SLOPE_RAILINGS);
        if (surface->GetBaseZ() < height || showUndergroundRailings)
            return true;
    }
    else if (pathEl.IsSloped())
    {
        // Diagonal path
        if (surface->GetSlope() != PathSlopeToLandSlope[pathEl.GetSlopeDirection()])
        {
            return true;
        }
    }
    else if (surface->GetSlope() != TILE_ELEMENT_SLOPE_FLAT)
    {
        return true;
    }
    return false;
}

static void PaintPatrolAreas(paint_session& session, const PathElement& pathEl)
{
    if (gStaffDrawPatrolAreas != 0xFFFF)
    {
        auto staffIndex = gStaffDrawPatrolAreas;
        auto staffType = static_cast<StaffType>(staffIndex & 0x7FFF);
        auto is_staff_list = staffIndex & 0x8000;
        auto patrolColour = COLOUR_LIGHT_BLUE;

        if (!is_staff_list)
        {
            Staff* staff = GetEntity<Staff>(staffIndex);
            if (staff == nullptr)
            {
                log_error("Invalid staff index for draw patrol areas!");
            }
            else
            {
                if (!staff->IsPatrolAreaSet(session.MapPosition))
                {
                    patrolColour = COLOUR_GREY;
                }
                staffType = staff->AssignedStaffType;
            }
        }

        if (staff_is_patrol_area_set_for_type(staffType, session.MapPosition))
        {
            uint32_t baseImageIndex = SPR_TERRAIN_STAFF;
            auto patrolAreaBaseZ = pathEl.GetBaseZ();
            if (pathEl.IsSloped())
            {
                baseImageIndex = SPR_TERRAIN_STAFF_SLOPED + ((pathEl.GetSlopeDirection() + session.CurrentRotation) & 3);
                patrolAreaBaseZ += 16;
            }

            auto imageId = ImageId(baseImageIndex, patrolColour);
            PaintAddImageAsParent(session, imageId, { 16, 16, patrolAreaBaseZ + 2 }, { 1, 1, 0 });
        }
    }
}

static void PaintHeightMarkers(paint_session& session, const PathElement& pathEl)
{
    if (PaintShouldShowHeightMarkers(session, VIEWPORT_FLAG_PATH_HEIGHTS))
    {
        uint16_t heightMarkerBaseZ = pathEl.GetBaseZ() + 3;
        if (pathEl.IsSloped())
        {
            heightMarkerBaseZ += 8;
        }

        uint32_t baseImageIndex = SPR_HEIGHT_MARKER_BASE;
        baseImageIndex += heightMarkerBaseZ / 16;
        baseImageIndex += get_height_marker_offset();
        baseImageIndex -= gMapBaseZ;
        auto imageId = ImageId(baseImageIndex, COLOUR_GREY);
        PaintAddImageAsParent(session, imageId, { 16, 16, heightMarkerBaseZ }, { 1, 1, 0 });
    }
}

static void PaintLampLightEffects(paint_session& session, const PathElement& pathEl, uint16_t height)
{
#ifdef __ENABLE_LIGHTFX__
    if (lightfx_is_available())
    {
        if (pathEl.HasAddition() && !(pathEl.IsBroken()))
        {
            auto* pathAddEntry = pathEl.GetAdditionEntry();
            if (pathAddEntry != nullptr && pathAddEntry->flags & PATH_BIT_FLAG_LAMP)
            {
                if (!(pathEl.GetEdges() & EDGE_NE))
                {
                    lightfx_add_3d_light_magic_from_drawing_tile(session.MapPosition, -16, 0, height + 23, LightType::Lantern3);
                }
                if (!(pathEl.GetEdges() & EDGE_SE))
                {
                    lightfx_add_3d_light_magic_from_drawing_tile(session.MapPosition, 0, 16, height + 23, LightType::Lantern3);
                }
                if (!(pathEl.GetEdges() & EDGE_SW))
                {
                    lightfx_add_3d_light_magic_from_drawing_tile(session.MapPosition, 16, 0, height + 23, LightType::Lantern3);
                }
                if (!(pathEl.GetEdges() & EDGE_NW))
                {
                    lightfx_add_3d_light_magic_from_drawing_tile(session.MapPosition, 0, -16, height + 23, LightType::Lantern3);
                }
            }
        }
    }
#endif
}

/**
 * rct2: 0x0006A3590
 */
void PaintPath(paint_session& session, uint16_t height, const PathElement& tileElement)
{
    session.InteractionType = ViewportInteractionItem::Footpath;

    ImageId imageTemplate, sceneryImageTemplate;
    if (gTrackDesignSaveMode)
    {
        // Do not display queues for other rides
        if (tileElement.IsQueue() && tileElement.GetRideIndex() != gTrackDesignSaveRideIndex)
        {
            return;
        }

        if (!track_design_save_contains_tile_element(reinterpret_cast<const TileElement*>(&tileElement)))
        {
            imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette46);
        }
    }

    if (session.ViewFlags & VIEWPORT_FLAG_HIGHLIGHT_PATH_ISSUES)
    {
        imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette46);
    }

    if (tileElement.AdditionIsGhost())
    {
        sceneryImageTemplate = ImageId().WithRemap(FilterPaletteID::Palette44);
    }

    if (tileElement.IsGhost())
    {
        session.InteractionType = ViewportInteractionItem::None;
        imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette44);
    }
    else if (TileInspector::IsElementSelected(reinterpret_cast<const TileElement*>(&tileElement)))
    {
        imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette44);
        sceneryImageTemplate = ImageId().WithRemap(FilterPaletteID::Palette44);
    }

    // For debugging purpose, show blocked tiles with a colour
    if (gPaintBlockedTiles && tileElement.IsBlockedByVehicle())
    {
        imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette46);
    }

    // Draw wide flags as ghosts, leaving only the "walkable" paths to be drawn normally
    if (gPaintWidePathsAsGhost && tileElement.IsWide())
    {
        imageTemplate = ImageId().WithRemap(FilterPaletteID::Palette44);
    }

    PaintPatrolAreas(session, tileElement);
    PaintHeightMarkers(session, tileElement);

    auto hasSupports = ShouldDrawSupports(session, tileElement, height);
    auto pathPaintInfo = GetFootpathPaintInfo(tileElement);
    if (pathPaintInfo.SupportType == RailingEntrySupportType::Pole)
    {
        path_paint_pole_support(session, tileElement, height, pathPaintInfo, hasSupports, imageTemplate, sceneryImageTemplate);
    }
    else
    {
        path_paint_box_support(session, tileElement, height, pathPaintInfo, hasSupports, imageTemplate, sceneryImageTemplate);
    }

    PaintLampLightEffects(session, tileElement, height);
}

void path_paint_box_support(
    paint_session& session, const PathElement& pathElement, int32_t height, const FootpathPaintInfo& pathPaintInfo,
    bool hasSupports, ImageId imageTemplate, ImageId sceneryImageTemplate)
{
    // Rol edges around rotation
    uint8_t edges = ((pathElement.GetEdges() << session.CurrentRotation) & 0xF)
        | (((pathElement.GetEdges()) << session.CurrentRotation) >> 4);

    uint8_t corners = (((pathElement.GetCorners()) << session.CurrentRotation) & 0xF)
        | (((pathElement.GetCorners()) << session.CurrentRotation) >> 4);

    CoordsXY boundBoxOffset = { stru_98D804[edges][0], stru_98D804[edges][1] };
    CoordsXY boundBoxSize = { stru_98D804[edges][2], stru_98D804[edges][3] };

    uint16_t edi = edges | (corners << 4);

    ImageIndex surfaceBaseImageIndex = pathPaintInfo.SurfaceImageId;
    if (pathElement.IsSloped())
    {
        auto directionOffset = (pathElement.GetSlopeDirection() + session.CurrentRotation)
            & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK;
        surfaceBaseImageIndex += 16 + directionOffset;
    }
    else
    {
        surfaceBaseImageIndex += byte_98D6E0[edi];
    }

    if (!session.DidPassSurface)
    {
        boundBoxOffset.x = 3;
        boundBoxOffset.y = 3;
        boundBoxSize.x = 26;
        boundBoxSize.y = 26;
    }

    // By default, add 1 to the z bounding box to always clip above the surface
    uint8_t boundingBoxZOffset = 1;

    // If we are on the same tile as a straight track, add the offset 2 so we
    //  can clip above gravel part of the track sprite
    if (session.TrackElementOnSameHeight != nullptr)
    {
        if (session.TrackElementOnSameHeight->AsTrack()->GetTrackType() == TrackElemType::Flat)
        {
            boundingBoxZOffset = 2;
        }
    }

    if (!hasSupports || !session.DidPassSurface)
    {
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(surfaceBaseImageIndex), { 0, 0, height }, { boundBoxSize, 0 },
            { boundBoxOffset, height + boundingBoxZOffset });
    }
    else
    {
        ImageIndex bridgeBaseImageIndex;
        if (pathElement.IsSloped())
        {
            auto directionOffset
                = ((pathElement.GetSlopeDirection() + session.CurrentRotation) & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK);
            bridgeBaseImageIndex = pathPaintInfo.BridgeImageId + 51 + directionOffset;
        }
        else
        {
            bridgeBaseImageIndex = byte_98D8A4[edges] + pathPaintInfo.BridgeImageId + 49;
        }

        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(bridgeBaseImageIndex), { 0, 0, height }, { boundBoxSize, 0 },
            { boundBoxOffset, height + boundingBoxZOffset });

        if (pathElement.IsQueue() || (pathPaintInfo.RailingFlags & RAILING_ENTRY_FLAG_DRAW_PATH_OVER_SUPPORTS))
        {
            PaintAddImageAsChild(
                session, imageTemplate.WithIndex(surfaceBaseImageIndex), { 0, 0, height },
                { boundBoxSize.x, boundBoxSize.y, 0 }, { boundBoxOffset.x, boundBoxOffset.y, height + boundingBoxZOffset });
        }
    }

    sub_6A3F61(session, pathElement, edi, height, pathPaintInfo, imageTemplate, sceneryImageTemplate, hasSupports);

    uint16_t ax = 0;
    if (pathElement.IsSloped())
    {
        ax = ((pathElement.GetSlopeDirection() + session.CurrentRotation) & 0x3) + 1;
    }

    auto supportType = byte_98D8A4[edges] == 0 ? 0 : 1;
    path_a_supports_paint_setup(session, supportType, ax, height, imageTemplate, pathPaintInfo, nullptr);

    height += 32;
    if (pathElement.IsSloped())
    {
        height += 16;
    }

    paint_util_set_general_support_height(session, height, 0x20);

    if (pathElement.IsQueue() || (pathElement.GetEdgesAndCorners() != 0xFF && hasSupports))
    {
        paint_util_set_segment_support_height(session, SEGMENTS_ALL, 0xFFFF, 0);
        return;
    }

    if (pathElement.GetEdgesAndCorners() == 0xFF)
    {
        paint_util_set_segment_support_height(session, SEGMENT_C8 | SEGMENT_CC | SEGMENT_D0 | SEGMENT_D4, 0xFFFF, 0);
        return;
    }

    paint_util_set_segment_support_height(session, SEGMENT_C4, 0xFFFF, 0);

    if (edges & 1)
    {
        paint_util_set_segment_support_height(session, SEGMENT_CC, 0xFFFF, 0);
    }

    if (edges & 2)
    {
        paint_util_set_segment_support_height(session, SEGMENT_D4, 0xFFFF, 0);
    }

    if (edges & 4)
    {
        paint_util_set_segment_support_height(session, SEGMENT_D0, 0xFFFF, 0);
    }

    if (edges & 8)
    {
        paint_util_set_segment_support_height(session, SEGMENT_C8, 0xFFFF, 0);
    }
}

void path_paint_pole_support(
    paint_session& session, const PathElement& pathElement, int16_t height, const FootpathPaintInfo& pathPaintInfo,
    bool hasSupports, ImageId imageTemplate, ImageId sceneryImageTemplate)
{
    // Rol edges around rotation
    uint8_t edges = ((pathElement.GetEdges() << session.CurrentRotation) & 0xF)
        | (((pathElement.GetEdges()) << session.CurrentRotation) >> 4);

    CoordsXY boundBoxOffset = { stru_98D804[edges][0], stru_98D804[edges][1] };

    CoordsXY boundBoxSize = { stru_98D804[edges][2], stru_98D804[edges][3] };

    uint8_t corners = (((pathElement.GetCorners()) << session.CurrentRotation) & 0xF)
        | (((pathElement.GetCorners()) << session.CurrentRotation) >> 4);

    uint16_t edi = edges | (corners << 4);

    ImageIndex surfaceBaseImageIndex = pathPaintInfo.SurfaceImageId;
    if (pathElement.IsSloped())
    {
        auto directionOffset
            = ((pathElement.GetSlopeDirection() + session.CurrentRotation) & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK);
        surfaceBaseImageIndex += 16 + directionOffset;
    }
    else
    {
        surfaceBaseImageIndex += byte_98D6E0[edi];
    }

    // Below Surface
    if (!session.DidPassSurface)
    {
        boundBoxOffset.x = 3;
        boundBoxOffset.y = 3;
        boundBoxSize.x = 26;
        boundBoxSize.y = 26;
    }

    // By default, add 1 to the z bounding box to always clip above the surface
    uint8_t boundingBoxZOffset = 1;

    // If we are on the same tile as a straight track, add the offset 2 so we
    //  can clip above gravel part of the track sprite
    if (session.TrackElementOnSameHeight != nullptr)
    {
        if (session.TrackElementOnSameHeight->AsTrack()->GetTrackType() == TrackElemType::Flat)
        {
            boundingBoxZOffset = 2;
        }
    }

    if (!hasSupports || !session.DidPassSurface)
    {
        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(surfaceBaseImageIndex), { 0, 0, height }, { boundBoxSize.x, boundBoxSize.y, 0 },
            { boundBoxOffset.x, boundBoxOffset.y, height + boundingBoxZOffset });
    }
    else
    {
        ImageIndex bridgeBaseImageIndex;
        if (pathElement.IsSloped())
        {
            bridgeBaseImageIndex = ((pathElement.GetSlopeDirection() + session.CurrentRotation)
                                    & FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK)
                + pathPaintInfo.BridgeImageId + 16;
        }
        else
        {
            bridgeBaseImageIndex = edges + pathPaintInfo.BridgeImageId;
        }

        PaintAddImageAsParent(
            session, imageTemplate.WithIndex(bridgeBaseImageIndex), { 0, 0, height }, { boundBoxSize, 0 },
            { boundBoxOffset, height + boundingBoxZOffset });

        if (pathElement.IsQueue() || (pathPaintInfo.RailingFlags & RAILING_ENTRY_FLAG_DRAW_PATH_OVER_SUPPORTS))
        {
            PaintAddImageAsChild(
                session, imageTemplate.WithIndex(surfaceBaseImageIndex), { 0, 0, height },
                { boundBoxSize.x, boundBoxSize.y, 0 }, { boundBoxOffset.x, boundBoxOffset.y, height + boundingBoxZOffset });
        }
    }

    sub_6A3F61(
        session, pathElement, edi, height, pathPaintInfo, imageTemplate, sceneryImageTemplate,
        hasSupports); // TODO: arguments

    uint16_t ax = 0;
    if (pathElement.IsSloped())
    {
        ax = 8;
    }

    uint8_t supports[] = {
        6,
        8,
        7,
        5,
    };

    for (int8_t i = 3; i > -1; --i)
    {
        if (!(edges & (1 << i)))
        {
            // Only colour the supports if not already remapped (e.g. ghost remap)
            auto supportColour = pathPaintInfo.SupportColour;
            if (supportColour != COLOUR_NULL && !imageTemplate.IsRemap())
            {
                imageTemplate = ImageId().WithPrimary(supportColour);
            }
            path_b_supports_paint_setup(session, supports[i], ax, height, imageTemplate, pathPaintInfo);
        }
    }

    height += 32;
    if (pathElement.IsSloped())
    {
        height += 16;
    }

    paint_util_set_general_support_height(session, height, 0x20);

    if (pathElement.IsQueue() || (pathElement.GetEdgesAndCorners() != 0xFF && hasSupports))
    {
        paint_util_set_segment_support_height(session, SEGMENTS_ALL, 0xFFFF, 0);
        return;
    }

    if (pathElement.GetEdgesAndCorners() == 0xFF)
    {
        paint_util_set_segment_support_height(session, SEGMENT_C8 | SEGMENT_CC | SEGMENT_D0 | SEGMENT_D4, 0xFFFF, 0);
        return;
    }

    paint_util_set_segment_support_height(session, SEGMENT_C4, 0xFFFF, 0);

    if (edges & EDGE_NE)
    {
        paint_util_set_segment_support_height(session, SEGMENT_CC, 0xFFFF, 0);
    }

    if (edges & EDGE_SE)
    {
        paint_util_set_segment_support_height(session, SEGMENT_D4, 0xFFFF, 0);
    }

    if (edges & EDGE_SW)
    {
        paint_util_set_segment_support_height(session, SEGMENT_D0, 0xFFFF, 0);
    }

    if (edges & EDGE_NW)
    {
        paint_util_set_segment_support_height(session, SEGMENT_C8, 0xFFFF, 0);
    }
}
