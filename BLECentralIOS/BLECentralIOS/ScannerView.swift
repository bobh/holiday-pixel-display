//
//  ScannerView.swift
//  BLECentralIOS
//
//  Displays all discovered Holiday Display boards.
//  Waits 15 seconds on appear to let peripherals boot and start advertising,
//  then auto-scans. User can tap Scan/Stop at any time.
//

import SwiftUI

struct ScannerView: View {
    var bleManager: BLEManager

    var body: some View {
        List {
            statusSection
            boardsSection
        }
        .navigationTitle("Holiday Display Boards")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button(bleManager.isScanning ? "Stop" : "Scan") {
                    if bleManager.isScanning {
                        bleManager.stopScanning()
                    } else {
                        bleManager.startScanning()
                    }
                }
            }
        }
        // 15-second delay allows peripherals to boot and begin advertising
        .task {
            try? await Task.sleep(for: .seconds(15))
            bleManager.startScanning()
        }
    }

    // MARK: - Status row

    @ViewBuilder
    private var statusSection: some View {
        Section {
            HStack(spacing: 8) {
                if bleManager.isScanning {
                    ProgressView()
                }
                Text(bleManager.connectionStatus)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: - Board list

    @ViewBuilder
    private var boardsSection: some View {
        Section("Boards (\(bleManager.discoveredBoards.count))") {
            if bleManager.discoveredBoards.isEmpty {
                Text(bleManager.isScanning
                     ? "Searching for boards..."
                     : "No boards found. Tap Scan to search.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                ForEach(bleManager.discoveredBoards.sorted(by: { $0.boardID < $1.boardID })) { board in
                    NavigationLink(value: board) {
                        BoardRowView(board: board)
                    }
                }
            }
        }
    }
}

// MARK: - Board row

struct BoardRowView: View {
    let board: DiscoveredBoard

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(board.displayName)
                    .font(.headline)
                Spacer()
                Text(board.configStatus.label)
                    .font(.caption)
                    .foregroundStyle(board.configStatus.isConfigured ? .green : .orange)
            }
            Text(board.detailText)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text("Signal: \(board.rssi) dBm")
                .font(.caption2)
                .foregroundStyle(.tertiary)
        }
        .padding(.vertical, 2)
    }
}
