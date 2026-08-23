#ifndef CALIBRATION_BOARD_FACTORY_H
#define CALIBRATION_BOARD_FACTORY_H

#include "calibration_board.h"
#include "fanuc_irvision_board.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file calibration_board_factory.h
 * @brief CalibrationBoardFactory — centralised allocator for CalibrationBoard subclasses.
 */

namespace calib {

/**
 * @enum CalibrationBoardType
 * @brief Enumerates the calibration board pattern types the factory can construct.
 */
enum class CalibrationBoardType {
    FanucIRvision,
    // Halcon,
    // ChArUco,
    // ChessBoard,
};

/**
 * @class CalibrationBoardFactory
 * @brief Centralised allocator for CalibrationBoard subclasses. Returns base-class
 *        std::unique_ptr<CalibrationBoard> so callers can stay board-agnostic.
 *
 * When type-specific accessors are needed (e.g. FanucIRvisionBoard::targetIndices),
 * construct the subclass directly instead of going through the factory.
 */
class CalibrationBoardFactory
{
public:
    /**
     * @brief Default-constructs a board of the given type using that type's own defaults.
     * @param[in] type the calibration board pattern to instantiate
     * @return a new board instance, or nullptr if `type` is unrecognised
     */
    static std::unique_ptr<CalibrationBoard> create(CalibrationBoardType type);

    /**
     * @brief Constructs a FanucIRvisionBoard with explicit params.
     * @param[in] params board geometry/detection parameters
     * @return a new FanucIRvisionBoard, held via its base-class pointer
     */
    static std::unique_ptr<CalibrationBoard>
        createFanucIRvision(const FanucIRvisionBoard::Params& params = FanucIRvisionBoard::Params{});

    /**
     * @brief Constructs a board from a named preset. Recognised presets:
     *        "iRvision5mm", "iRvision11mm", "iRvision15mm", "iRvision22mm", "iRvision30mm".
     * @param[in] presetName the preset identifier
     * @return a new board instance, or nullptr for unknown names
     */
    static std::unique_ptr<CalibrationBoard> createFromPreset(const std::string& presetName);

    /// @return the list of preset names recognised by createFromPreset().
    static std::vector<std::string> availablePresets();

    /**
     * @brief Reconstructs a board instance from JSON produced by CalibrationBoard::toJson(),
     *        dispatching on the top-level "type" field.
     * @param[in] json serialised board JSON
     * @return a new board instance, or nullptr if the type is unknown or the parsed Params fail validation
     */
    static std::unique_ptr<CalibrationBoard> createFromJson(const std::string& json);
};

} // namespace calib

#endif // CALIBRATION_BOARD_FACTORY_H
