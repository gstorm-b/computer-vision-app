#include "calibration_board_factory.h"

#include <opencv2/core/persistence.hpp>

#include <stdexcept>

namespace calib {

/// Default-constructs a board of the given type using that type's own defaults, dispatching
/// on `type`.
/// @return a new board instance, or nullptr if `type` is unrecognised
std::unique_ptr<CalibrationBoard>
CalibrationBoardFactory::create(CalibrationBoardType type)
{
    switch (type) {
    case CalibrationBoardType::FanucIRvision:
        return createFanucIRvision();
    }
    return nullptr;
}

/// Constructs a FanucIRvisionBoard with explicit `params`.
std::unique_ptr<CalibrationBoard>
CalibrationBoardFactory::createFanucIRvision(const FanucIRvisionBoard::Params& params)
{
    return std::make_unique<FanucIRvisionBoard>(params);
}

/// Constructs a FanucIRvisionBoard from a named preset ("iRvision-5mm", "iRvision-11.5mm",
/// "iRvision-15mm", "iRvision-22.5mm", or "iRvision-30mm"; see availablePresets()).
/// @return a new board instance, or nullptr for an unrecognised preset name
std::unique_ptr<CalibrationBoard>
CalibrationBoardFactory::createFromPreset(const std::string& presetName)
{
    if (presetName == "iRvision-5mm")
        return createFanucIRvision(FanucIRvisionBoard::iRvisionPattern5mm);
    if (presetName == "iRvision-11.5mm")
        return createFanucIRvision(FanucIRvisionBoard::iRvisionPattern11m);
    if (presetName == "iRvision-15mm")
        return createFanucIRvision(FanucIRvisionBoard::iRvisionPattern15mm);
    if (presetName == "iRvision-22.5mm")
        return createFanucIRvision(FanucIRvisionBoard::iRvisionPattern22mm);
    if (presetName == "iRvision-30mm")
        return createFanucIRvision(FanucIRvisionBoard::iRvisionPattern30mm);
    return nullptr;
}

/// Reconstructs a board instance from JSON produced by CalibrationBoard::toJson(),
/// dispatching on the top-level "type" field (currently only "FanucIRvision" is recognised).
/// @return a new board instance, or nullptr if the type is unknown, the FileStorage cannot
///         be opened, or the parsed Params fail validation
std::unique_ptr<CalibrationBoard>
CalibrationBoardFactory::createFromJson(const std::string& json)
{
    cv::FileStorage fs(json,
                       cv::FileStorage::READ | cv::FileStorage::MEMORY
                                             | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened()) return nullptr;

    std::string type;
    fs["type"] >> type;
    fs.release();

    if (type == "FanucIRvision") {
        try {
            return std::make_unique<FanucIRvisionBoard>(
                FanucIRvisionBoard::paramsFromJson(json));
        } catch (const std::invalid_argument&) {
            return nullptr;
        }
    }
    return nullptr;
}

/// @return the list of preset names recognised by createFromPreset().
std::vector<std::string> CalibrationBoardFactory::availablePresets()
{
    return {
        "iRvision-5mm",
        "iRvision-11.5mm",
        "iRvision-15mm",
        "iRvision-22.5mm",
        "iRvision-30mm"
    };
}

} // namespace calib
