/**
 * @file setting.h
 * @brief Enum for the network setting of this MPC execution
 *
 */

#pragma once

namespace orq::service {

enum class Setting { SAME, LAN, WAN };

/**
 * @brief Parse a string and return the corresponding Setting object.
 *
 * @param setting_str The string representation of the setting.
 * @return Setting The parsed Setting object.
 * @throws std::invalid_argument if the string does not match any Setting.
 */
Setting parse_setting(const std::string& setting_str) {
    if (setting_str == "same") {
        return Setting::SAME;
    } else if (setting_str == "lan") {
        return Setting::LAN;
    } else if (setting_str == "wan") {
        return Setting::WAN;
    } else {
        throw std::invalid_argument("Invalid setting: " + setting_str);
    }
}

}  // namespace orq::service