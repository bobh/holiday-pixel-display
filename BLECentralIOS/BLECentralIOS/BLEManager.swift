//
//  BLEManager.swift
//  BLECentralIOS
//
//  BLE Central Manager for OrthancTower communication
//

import Foundation
import CoreBluetooth
import SwiftUI
import Observation

// MARK: - UUIDs matching BLEPeripheral.ino
enum HolidayBLE {
    static let serviceUUID        = CBUUID(string: "19B10000-E8F2-537E-4F6C-D104768A1214")
    static let deviceInfoUUID     = CBUUID(string: "19B10010-E8F2-537E-4F6C-D104768A1214")
    static let batteryUUID        = CBUUID(string: "19B10011-E8F2-537E-4F6C-D104768A1214")
    static let effectIdUUID       = CBUUID(string: "19B10001-E8F2-537E-4F6C-D104768A1214")
    static let brightnessUUID     = CBUUID(string: "19B10002-E8F2-537E-4F6C-D104768A1214")
    static let speedUUID          = CBUUID(string: "19B10003-E8F2-537E-4F6C-D104768A1214")
    static let colorPrimaryUUID   = CBUUID(string: "19B10004-E8F2-537E-4F6C-D104768A1214")
    static let colorSecondaryUUID = CBUUID(string: "19B10005-E8F2-537E-4F6C-D104768A1214")
    static let flagsUUID          = CBUUID(string: "19B10006-E8F2-537E-4F6C-D104768A1214")
    static let audioTrackUUID     = CBUUID(string: "19B10007-E8F2-537E-4F6C-D104768A1214")
    static let saveConfigUUID     = CBUUID(string: "19B10008-E8F2-537E-4F6C-D104768A1214")
}

// MARK: - Device Type (per protocol v0.1)
enum DeviceType: UInt8 {
    case unknown    = 0x00
    case nano33BLE  = 0x01
    case esp32S3    = 0x02

    var name: String {
        switch self {
        case .unknown:   return "Unknown"
        case .nano33BLE: return "Nano 33 BLE"
        case .esp32S3:   return "ESP32-S3"
        }
    }
}

// MARK: - Capability Flags (per protocol v0.1)
struct CapabilityFlags: OptionSet {
    let rawValue: UInt8

    static let rgbw  = CapabilityFlags(rawValue: 1 << 0)
    static let fram  = CapabilityFlags(rawValue: 1 << 1)
    static let audio = CapabilityFlags(rawValue: 1 << 2)
    static let flash = CapabilityFlags(rawValue: 1 << 3)

    var description: String {
        var caps: [String] = []
        if contains(.rgbw)  { caps.append("RGBW") }
        if contains(.fram)  { caps.append("FRAM") }
        if contains(.audio) { caps.append("Audio") }
        if contains(.flash) { caps.append("Flash") }
        return caps.joined(separator: ", ")
    }
}

// MARK: - Config Status (per protocol v0.1, advertising byte 7)
// Option A: Reserved byte repurposed for configuration state.
// 0x00 = Unconfigured (peripheral shows YELLOW LED)
// 0x01 = Configured   (peripheral shows GREEN LED)
// 0xFF = Factory Reset
enum ConfigStatus: UInt8 {
    case unconfigured = 0x00
    case configured   = 0x01
    case factoryReset = 0xFF

    var label: String {
        switch self {
        case .unconfigured: return "⚠️ Unconfigured"
        case .configured:   return "✓ Configured"
        case .factoryReset: return "Factory Reset"
        }
    }

    var isConfigured: Bool { self == .configured }
}

// MARK: - Discovered Board (parsed from advertising data)
struct DiscoveredBoard: Identifiable, Hashable {
    let id: UUID            // = peripheral.identifier
    let peripheral: CBPeripheral
    let boardID: UInt8      // UI board number, resolved from advertised ID + peripheral UUID
    let advertisedBoardID: UInt8
    let deviceType: DeviceType
    let firmwareVersion: String
    let pixelCount: UInt8
    let capabilities: CapabilityFlags
    var configStatus: ConfigStatus
    var rssi: Int

    static func == (lhs: DiscoveredBoard, rhs: DiscoveredBoard) -> Bool { lhs.id == rhs.id }
    func hash(into hasher: inout Hasher) { hasher.combine(id) }

    var displayName: String {
        boardID == 0 ? "Board 0 (Master)" : "Board \(boardID)"
    }

    var detailText: String {
        "\(deviceType.name) | FW v\(firmwareVersion) | \(pixelCount) pixels"
    }

    var shortUUID: String {
        String(id.uuidString.prefix(8))
    }
}

// MARK: - Effect definitions matching firmware enum
enum DisplayEffect: UInt8, CaseIterable, Identifiable {
    case fire          = 0
    case candle        = 1
    case ember         = 2
    case sparkle       = 3
    case warmWhite     = 4
    case lotrColdWhite = 10
    case lotrPalantir  = 11
    case lotrManyColor = 12

    var id: UInt8 { rawValue }

    var name: String {
        switch self {
        case .fire:          return "Fire"
        case .candle:        return "Candle"
        case .ember:         return "Ember"
        case .sparkle:       return "Sparkle"
        case .warmWhite:     return "Warm White"
        case .lotrColdWhite: return "LOTR Cold White"
        case .lotrPalantir:  return "LOTR Palantir"
        case .lotrManyColor: return "LOTR Many Colors"
        }
    }
}

@MainActor
@Observable
final class BLEManager: NSObject {
    // MARK: - Observable Properties

    var isScanning = false
    var isConnected = false
    var connectionStatus = "Initializing..."
    var deviceName = ""
    var deviceInfo = ""

    // Advertising data for the currently connected board
    var boardID: UInt8 = 0
    var deviceType: DeviceType = .unknown
    var firmwareVersion: String = ""
    var pixelCount: UInt8 = 0
    var capabilities: CapabilityFlags = []

    // All boards discovered during current scan
    var discoveredBoards: [DiscoveredBoard] = []

    // Current device state (read back from peripheral)
    var currentEffect: DisplayEffect = .fire
    var brightness: Double = 128
    var speed: Double = 128
    var primaryColor: Color = .white
    var secondaryColor: Color = .black
    var batteryMv: UInt16 = 0
    var isSavingConfig = false

    // MARK: - BLE Properties
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?

    // Characteristic references (populated on discovery)
    private var charEffectId: CBCharacteristic?
    private var charBrightness: CBCharacteristic?
    private var charSpeed: CBCharacteristic?
    private var charColorPrimary: CBCharacteristic?
    private var charColorSecondary: CBCharacteristic?
    private var charFlags: CBCharacteristic?
    private var charAudioTrack: CBCharacteristic?
    private var charSaveConfig: CBCharacteristic?
    private var charDeviceInfo: CBCharacteristic?
    private var charBattery: CBCharacteristic?
    private var disconnectAfterSaveAck = false
    private let boardNumberAliasesKey = "HolidayDisplay.boardNumberAliases.v1"
    private var boardNumberByPeripheralID: [UUID: UInt8] = [:]

    // MARK: - Initialization
    override init() {
        super.init()
        loadBoardNumberAliases()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - Public Methods

    func startScanning() {
        guard centralManager.state == .poweredOn else {
            connectionStatus = "Bluetooth not ready"
            return
        }
        connectionStatus = discoveredBoards.isEmpty
            ? "Scanning for Holiday Display boards..."
            : "Scanning — showing \(discoveredBoards.count) known board(s)"
        isScanning = true
        centralManager.scanForPeripherals(withServices: [HolidayBLE.serviceUUID], options: nil)
    }

    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
        if !isConnected {
            connectionStatus = discoveredBoards.isEmpty
                ? "Scan stopped — no boards found"
                : "Found \(discoveredBoards.count) board(s)"
        }
    }

    func connect(to board: DiscoveredBoard) {
        boardID = board.boardID
        deviceType = board.deviceType
        firmwareVersion = board.firmwareVersion
        pixelCount = board.pixelCount
        capabilities = board.capabilities
        deviceName = board.displayName
        deviceInfo = board.detailText
        connectedPeripheral = board.peripheral
        connectionStatus = "Connecting to \(board.displayName)..."
        centralManager.connect(board.peripheral, options: nil)
    }

    func disconnect() {
        guard let peripheral = connectedPeripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
    }

    // MARK: - Write Commands

    func writeEffect(_ effect: DisplayEffect) {
        guard let char = charEffectId else { return }
        var value = effect.rawValue
        connectedPeripheral?.writeValue(Data(bytes: &value, count: 1), for: char, type: .withResponse)
        currentEffect = effect
    }

    func writeBrightness(_ value: UInt8) {
        guard let char = charBrightness else { return }
        var v = value
        connectedPeripheral?.writeValue(Data(bytes: &v, count: 1), for: char, type: .withResponse)
    }

    func writeSpeed(_ value: UInt8) {
        guard let char = charSpeed else { return }
        var v = value
        connectedPeripheral?.writeValue(Data(bytes: &v, count: 1), for: char, type: .withResponse)
    }

    func writePrimaryColor(_ color: Color) {
        guard let char = charColorPrimary else { return }
        var value = colorToRGB(color)
        connectedPeripheral?.writeValue(Data(bytes: &value, count: 4), for: char, type: .withResponse)
    }

    func writeSecondaryColor(_ color: Color) {
        guard let char = charColorSecondary else { return }
        var value = colorToRGB(color)
        connectedPeripheral?.writeValue(Data(bytes: &value, count: 4), for: char, type: .withResponse)
    }

    func saveConfig() {
        guard let char = charSaveConfig else { return }
        var value: UInt8 = 1
        connectedPeripheral?.writeValue(Data(bytes: &value, count: 1), for: char, type: .withResponse)
        markCurrentBoardConfigured()
    }

    // MARK: - Helpers

    // Optimistically mark the connected board as configured in the discovered list.
    // ScannerView will show the updated status when the user returns.
    private func markCurrentBoardConfigured() {
        guard let peripheral = connectedPeripheral else { return }
        if let idx = discoveredBoards.firstIndex(where: { $0.id == peripheral.identifier }) {
            discoveredBoards[idx].configStatus = .configured
        }
    }

    private func colorToRGB(_ color: Color) -> UInt32 {
        let resolved = UIColor(color)
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        resolved.getRed(&r, green: &g, blue: &b, alpha: &a)
        let ri = UInt32(r * 255) & 0xFF
        let gi = UInt32(g * 255) & 0xFF
        let bi = UInt32(b * 255) & 0xFF
        return (ri << 16) | (gi << 8) | bi
    }

    private func clearCharacteristics() {
        charEffectId = nil
        charBrightness = nil
        charSpeed = nil
        charColorPrimary = nil
        charColorSecondary = nil
        charFlags = nil
        charAudioTrack = nil
        charSaveConfig = nil
        charDeviceInfo = nil
        charBattery = nil
        isSavingConfig = false
        disconnectAfterSaveAck = false
    }

    private func displayBoardID(for peripheralID: UUID, advertisedBoardID: UInt8) -> UInt8 {
        if let existing = boardNumberByPeripheralID[peripheralID] {
            if existing != advertisedBoardID && !isBoardNumberAssigned(advertisedBoardID, excluding: peripheralID) {
                boardNumberByPeripheralID[peripheralID] = advertisedBoardID
                saveBoardNumberAliases()
                return advertisedBoardID
            }
            return existing
        }

        if !isBoardNumberAssigned(advertisedBoardID, excluding: peripheralID) {
            boardNumberByPeripheralID[peripheralID] = advertisedBoardID
            saveBoardNumberAliases()
            return advertisedBoardID
        }

        for candidate in 1...99 {
            let boardNumber = UInt8(candidate)
            if !isBoardNumberAssigned(boardNumber, excluding: peripheralID) {
                boardNumberByPeripheralID[peripheralID] = boardNumber
                saveBoardNumberAliases()
                return boardNumber
            }
        }

        return advertisedBoardID
    }

    private func isBoardNumberAssigned(_ boardNumber: UInt8, excluding peripheralID: UUID) -> Bool {
        boardNumberByPeripheralID.contains { item in
            item.key != peripheralID && item.value == boardNumber
        }
    }

    private func loadBoardNumberAliases() {
        guard let saved = UserDefaults.standard.dictionary(forKey: boardNumberAliasesKey) as? [String: Int] else {
            return
        }

        boardNumberByPeripheralID = saved.reduce(into: [:]) { result, item in
            guard let uuid = UUID(uuidString: item.key),
                  (0...99).contains(item.value) else { return }
            result[uuid] = UInt8(item.value)
        }
    }

    private func saveBoardNumberAliases() {
        let saved = boardNumberByPeripheralID.reduce(into: [String: Int]()) { result, item in
            result[item.key.uuidString] = Int(item.value)
        }
        UserDefaults.standard.set(saved, forKey: boardNumberAliasesKey)
    }
}

// MARK: - CBCentralManagerDelegate
extension BLEManager: CBCentralManagerDelegate {

    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                connectionStatus = "Bluetooth ready"
            case .poweredOff:
                connectionStatus = "Bluetooth is off"
            case .unauthorized:
                connectionStatus = "Bluetooth not authorized"
            case .unsupported:
                connectionStatus = "Bluetooth not supported"
            default:
                connectionStatus = "Bluetooth unavailable"
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any],
                                    rssi RSSI: NSNumber) {
        Task { @MainActor in
            // Parse manufacturer data (per protocol v0.1 Section 5)
            guard let mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
                  mfgData.count >= 8 else { return }

            let companyID = UInt16(mfgData[0]) | (UInt16(mfgData[1]) << 8)
            guard companyID == 0xFFFF else { return }

            let advertisedBoardID = mfgData[2]
            let dtype     = DeviceType(rawValue: mfgData[3]) ?? .unknown
            let fwMajor   = mfgData[4] >> 4
            let fwMinor   = mfgData[4] & 0x0F
            let pixels    = mfgData[5]
            let caps      = CapabilityFlags(rawValue: mfgData[6])
            // Byte 7: config state (was Reserved 0x00 — old firmware treated as unconfigured)
            let cfgStatus = ConfigStatus(rawValue: mfgData[7]) ?? .unconfigured
            let displayBoardID = displayBoardID(for: peripheral.identifier, advertisedBoardID: advertisedBoardID)

            let board = DiscoveredBoard(
                id: peripheral.identifier,
                peripheral: peripheral,
                boardID: displayBoardID,
                advertisedBoardID: advertisedBoardID,
                deviceType: dtype,
                firmwareVersion: "\(fwMajor).\(fwMinor)",
                pixelCount: pixels,
                capabilities: caps,
                configStatus: cfgStatus,
                rssi: RSSI.intValue
            )

            if let idx = discoveredBoards.firstIndex(where: { $0.id == peripheral.identifier }) {
                discoveredBoards[idx] = board
            } else {
                discoveredBoards.append(board)
            }

            connectionStatus = "Found \(discoveredBoards.count) board(s)"
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            connectionStatus = "Connected, discovering services..."
            isConnected = true
            peripheral.delegate = self
            peripheral.discoverServices([HolidayBLE.serviceUUID])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            connectionStatus = "Failed to connect: \(error?.localizedDescription ?? "unknown")"
            isConnected = false
            connectedPeripheral = nil
            clearCharacteristics()
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            connectionStatus = "Disconnected"
            isConnected = false
            connectedPeripheral = nil
            clearCharacteristics()
        }
    }
}

// MARK: - CBPeripheralDelegate
extension BLEManager: CBPeripheralDelegate {

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error = error {
                connectionStatus = "Service error: \(error.localizedDescription)"
                return
            }
            guard let services = peripheral.services else { return }
            for service in services {
                peripheral.discoverCharacteristics(nil, for: service)
            }
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        Task { @MainActor in
            if let error = error {
                connectionStatus = "Characteristic error: \(error.localizedDescription)"
                return
            }
            guard let characteristics = service.characteristics else { return }

            for char in characteristics {
                switch char.uuid {
                case HolidayBLE.effectIdUUID:       charEffectId = char
                case HolidayBLE.brightnessUUID:     charBrightness = char
                case HolidayBLE.speedUUID:          charSpeed = char
                case HolidayBLE.colorPrimaryUUID:   charColorPrimary = char
                case HolidayBLE.colorSecondaryUUID: charColorSecondary = char
                case HolidayBLE.flagsUUID:          charFlags = char
                case HolidayBLE.audioTrackUUID:     charAudioTrack = char
                case HolidayBLE.saveConfigUUID:     charSaveConfig = char
                case HolidayBLE.deviceInfoUUID:     charDeviceInfo = char
                case HolidayBLE.batteryUUID:        charBattery = char
                default: break
                }

                if char.properties.contains(.read) {
                    peripheral.readValue(for: char)
                }
                if char.uuid == HolidayBLE.batteryUUID && char.properties.contains(.notify) {
                    peripheral.setNotifyValue(true, for: char)
                }
            }

            connectionStatus = "Ready"
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didUpdateValueFor characteristic: CBCharacteristic,
                                error: Error?) {
        guard error == nil, let data = characteristic.value else { return }

        Task { @MainActor in
            switch characteristic.uuid {
            case HolidayBLE.effectIdUUID:
                if let byte = data.first, let effect = DisplayEffect(rawValue: byte) {
                    self.currentEffect = effect
                }
            case HolidayBLE.brightnessUUID:
                if let byte = data.first {
                    self.brightness = Double(byte)
                }
            case HolidayBLE.speedUUID:
                if let byte = data.first {
                    self.speed = Double(byte)
                }
            case HolidayBLE.deviceInfoUUID:
                if let str = String(data: data, encoding: .utf8) {
                    self.deviceInfo = str
                }
            case HolidayBLE.batteryUUID:
                if data.count >= 2 {
                    self.batteryMv = UInt16(data[0]) | (UInt16(data[1]) << 8)
                }
            default:
                break
            }
        }
    }
}
