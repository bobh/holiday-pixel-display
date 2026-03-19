//
//  ControlView.swift
//  BLECentralIOS
//
//  Effect/brightness/speed/color control for a selected board.
//  Connects on appear, disconnects on disappear (back button or Save & Next).
//

import SwiftUI

struct ControlView: View {
    let board: DiscoveredBoard
    var bleManager: BLEManager

    var body: some View {
        List {
            connectionSection
            effectSection
            controlsSection
            colorsSection
            saveSection
            batterySection
        }
        .navigationTitle(board.displayName)
        .task {
            bleManager.connect(to: board)
        }
        .onDisappear {
            // Covers the back-button path; Save & Next calls disconnect() explicitly first
            bleManager.disconnect()
        }
    }

    // MARK: - Connection Status

    @ViewBuilder
    private var connectionSection: some View {
        Section("Connection") {
            HStack {
                Circle()
                    .fill(bleManager.isConnected ? Color.green : Color.orange)
                    .frame(width: 12, height: 12)
                Text(bleManager.isConnected ? bleManager.deviceName : "Connecting...")
                    .font(.headline)
                Spacer()
                if bleManager.isConnected {
                    Button("Disconnect") {
                        bleManager.disconnect()
                    }
                    .foregroundStyle(.red)
                }
            }

            Text(bleManager.connectionStatus)
                .font(.caption)
                .foregroundStyle(.secondary)

            if !bleManager.deviceInfo.isEmpty {
                Text(bleManager.deviceInfo)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }

            if !bleManager.capabilities.isEmpty {
                Text("Capabilities: \(bleManager.capabilities.description)")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: - Effect Selection

    @ViewBuilder
    private var effectSection: some View {
        Section("Effect") {
            effectButton(for: .fire)
            effectButton(for: .candle)
            effectButton(for: .ember)
            effectButton(for: .sparkle)
            effectButton(for: .warmWhite)
            effectButton(for: .lotrColdWhite)
            effectButton(for: .lotrPalantir)
            effectButton(for: .lotrManyColor)
        }
    }

    @ViewBuilder
    private func effectButton(for effect: DisplayEffect) -> some View {
        Button {
            bleManager.writeEffect(effect)
        } label: {
            HStack {
                Text(effect.name)
                    .foregroundStyle(.primary)
                Spacer()
                if bleManager.currentEffect == effect {
                    Image(systemName: "checkmark")
                        .foregroundStyle(.blue)
                }
            }
        }
        .disabled(!bleManager.isConnected)
    }

    // MARK: - Brightness & Speed

    @ViewBuilder
    private var controlsSection: some View {
        @Bindable var manager = bleManager
        Section("Controls") {
            VStack(alignment: .leading) {
                Text("Brightness: \(Int(manager.brightness))")
                    .font(.subheadline)
                Slider(value: $manager.brightness, in: 0...255, step: 1) { editing in
                    if !editing {
                        manager.writeBrightness(UInt8(manager.brightness))
                    }
                }
            }
            .disabled(!manager.isConnected)

            VStack(alignment: .leading) {
                Text("Speed: \(Int(manager.speed))")
                    .font(.subheadline)
                Slider(value: $manager.speed, in: 0...255, step: 1) { editing in
                    if !editing {
                        manager.writeSpeed(UInt8(manager.speed))
                    }
                }
            }
            .disabled(!manager.isConnected)
        }
    }

    // MARK: - Colors

    @ViewBuilder
    private var colorsSection: some View {
        @Bindable var manager = bleManager
        Section("Colors") {
            ColorPicker("Primary", selection: $manager.primaryColor)
                .disabled(!manager.isConnected)
                .onChange(of: manager.primaryColor) { _, newValue in
                    manager.writePrimaryColor(newValue)
                }

            ColorPicker("Secondary", selection: $manager.secondaryColor)
                .disabled(!manager.isConnected)
                .onChange(of: manager.secondaryColor) { _, newValue in
                    manager.writeSecondaryColor(newValue)
                }
        }
    }

    // MARK: - Save & Next

    @ViewBuilder
    private var saveSection: some View {
        Section {
            Button("Save & Next") {
                bleManager.saveConfig()
                bleManager.disconnect()
            }
            .disabled(!bleManager.isConnected)
        }
    }

    // MARK: - Battery

    @ViewBuilder
    private var batterySection: some View {
        if bleManager.batteryMv > 0 {
            Section("Battery") {
                Text("\(bleManager.batteryMv) mV")
                    .font(.caption)
            }
        }
    }
}
