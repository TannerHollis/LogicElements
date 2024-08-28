#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <sstream>

class le_JsonSettings {
protected:
    le_JsonSettings(const std::string& filename) : filename(filename) 
    {
        settings = nlohmann::json::object();
    }

    std::string getSetting(const std::string& key) const {
        if (settings.contains(key)) {
            return settings[key].get<std::string>();
        }
        return "";
    }

    void setSetting(const std::string& key, const std::string& value) {
        settings[key] = value;
        saveToFile();
    }

    std::string filename;
    nlohmann::json settings;

    bool loadFromFile() {
        std::ifstream file(filename);
        if (file.is_open()) {
            try {
                file >> settings;
            } catch (const nlohmann::json::parse_error& e) {
                file.close();
                return false;
            }
            file.close();
            return true;
        }
        return false;
    }

    void saveToFile() const {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << settings.dump(4);
            file.close();
        }
    }

    virtual void loadDefaultSettings() = 0;
};

class le_DeviceSettings : public le_JsonSettings {
public:
    le_DeviceSettings(const std::string& filename) : le_JsonSettings(filename) 
    {
        if (!loadFromFile()) 
        {
            loadDefaultSettings();
            saveToFile();
        }
    }

    std::string getActiveConfig() const {
        return getSetting("activeConfig");
    }

    void setActiveConfig(const std::string& value) {
        setSetting("activeConfig", value);
    }

    std::string getIPAddress() const {
        return getSetting("ip_addr");
    }

    void setIPAddress(const std::string& value) {
        setSetting("ip_addr", value);
    }

    bool getSocketEnable() const {
        return getSetting("socket_0_en") == "true";
    }

    void setSocketEnable(bool value) {
        setSetting("socket_0_en", value ? "true" : "false");
    }

    int getSocketPort() const {
        return std::stoi(getSetting("socket_0_port"));
    }

    void setSocketPort(int value) {
        setSetting("socket_0_port", std::to_string(value));
    }

    int getSocketRetryMS() const {
        return std::stoi(getSetting("socket_0_retry_ms"));
    }

    void setSocketRetryMS(int value) {
        setSetting("socket_0_retry_ms", std::to_string(value));
    }

    bool getSerialPortEnable() const {
        return getSetting("serial_0_en") == "true";
    }

    void setSerialPortEnable(bool value) {
        setSetting("serial_0_en", value ? "true" : "false");
    }

    int getSerialSpeed() const {
        return std::stoi(getSetting("serial_0_speed"));
    }

    void setSerialSpeed(int value) {
        setSetting("serial_0_speed", std::to_string(value));
    }
    
    std::string getSerialPort() const {
        return getSetting("serial_0_port");
    }

    void setSerialPort(const std::string& value) {
        setSetting("serial_0_port", value);
    }

protected:
    void loadDefaultSettings() {
        // Add default settings here
        setSetting("activeConfig", "example_config.json");
        setSetting("ip_addr", "192.168.0.196");
        setSetting("socket_0_en", "true");
        setSetting("socket_0_port", "502");
        setSetting("socket_0_retry_ms", "10000");
        setSetting("serial_0_en", "true");
        setSetting("serial_0_port", "COM4");
        setSetting("serial_0_speed", "115200");
    }
};