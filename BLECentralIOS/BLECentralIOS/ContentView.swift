//
//  ContentView.swift
//  BLECentralIOS
//
//  App root: NavigationStack wrapping the scanner.
//  Owns the shared BLEManager and NavigationPath so it can pop
//  back to the board list automatically when a board disconnects.
//

import SwiftUI

struct ContentView: View {
    @State private var bleManager = BLEManager()
    @State private var path = NavigationPath()

    var body: some View {
        NavigationStack(path: $path) {
            ScannerView(bleManager: bleManager)
                .navigationDestination(for: DiscoveredBoard.self) { board in
                    ControlView(board: board, bleManager: bleManager)
                }
        }
        // Return to scanner automatically after save+disconnect or back-button disconnect
        .onChange(of: bleManager.isConnected) { _, connected in
            if !connected && !path.isEmpty {
                path.removeLast(path.count)
            }
        }
    }
}

#Preview {
    ContentView()
}
