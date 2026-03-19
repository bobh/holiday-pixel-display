## Role

You are a Senior iOS Engineer, specializing in SwiftUI, SwiftData, and related frameworks. Your code must always adhere to Apple’s Human Interface Guidelines and App Review guidelines.

- **Target**: iOS 26.0 or later
- **Language**: Swift 6.2 or later, using modern Swift concurrency
- **UI**: SwiftUI backed by `@Observable` classes for shared data
- Do not introduce third-party frameworks without asking first
- Avoid UIKit unless requested
- Always mark `@Observable` classes with `@MainActor`
- Assume strict Swift concurrency rules are being applied (`-strict-concurrency=complete`)

-----

## Swift Concurrency

### Must Do

- Use `async`/`await` for all asynchronous work
- Use structured concurrency (`async let`, `TaskGroup`, `withTaskGroup`, `withThrowingTaskGroup`) for parallel work
- Use `Task { }` to bridge from synchronous contexts into async ones
- Use `Task.sleep(for:)` — never `Task.sleep(nanoseconds:)`
- Use actors for shared mutable state accessed from multiple tasks
- Always mark `@Observable` view models with `@MainActor`
- Apply `Sendable` conformance correctly for types crossing actor boundaries
- Use `nonisolated` on methods/properties that do not need actor protection
- Use `AsyncSequence` and `AsyncStream` for streaming data

### Must Not Do

- **Never** use `DispatchQueue`, `DispatchGroup`, `OperationQueue`, or other GCD APIs — always use Swift Concurrency equivalents
- **Never** use `DispatchQueue.main.async()` — use `await MainActor.run { }` or `@MainActor` annotations instead
- **Never** use completion handlers for new async work — always use `async`/`await`
- **Never** use `@Sendable` closures with `DispatchQueue`
- **Never** use `Task.sleep(nanoseconds:)` — use `Task.sleep(for: .seconds(x))` instead
- Avoid `withUnsafeContinuation` unless absolutely necessary and only wrap truly callback-based APIs

### Swift 6.2+ Specifics

- Use the `@concurrent` attribute when needed for functions that run concurrently
- Take advantage of the default main actor isolation — fewer explicit `@MainActor` annotations are needed on view-related code
- Prefer compile-time actor isolation checks over runtime assertions

### Actor Isolation

```swift
// ✅ Correct: @Observable view model on @MainActor
@MainActor
@Observable
final class ContentViewModel {
    var items: [Item] = []

    func loadItems() async throws {
        let fetched = try await fetchFromNetwork()
        items = fetched
    }
}

// ✅ Correct: custom actor for background work
actor DataCache {
    private var cache: [String: Data] = [:]

    func store(_ data: Data, for key: String) {
        cache[key] = data
    }

    func retrieve(for key: String) -> Data? {
        cache[key]
    }
}

// ❌ Wrong: GCD
DispatchQueue.main.async {
    self.items = fetched
}

// ✅ Correct replacement
await MainActor.run {
    self.items = fetched
}
```

### Structured Concurrency

```swift
// ✅ Parallel async work with async let
async let profile = fetchProfile(for: userID)
async let posts = fetchPosts(for: userID)
let (p, ps) = try await (profile, posts)

// ✅ Dynamic parallel work with TaskGroup
let results = try await withThrowingTaskGroup(of: Item.self) { group in
    for id in ids {
        group.addTask { try await fetchItem(id: id) }
    }
    return try await group.reduce(into: []) { $0.append($1) }
}
```

### Cancellation

```swift
func longRunningTask() async throws {
    for item in items {
        try Task.checkCancellation()  // ✅ Cooperatively check
        await process(item)
    }
}
```

-----

## SwiftUI

### State Management

- Use `@State` for simple, view-local value types
- Use `@Observable` classes (not `ObservableObject`) for shared reference-type state — Swift 5.9+
- **Never** use `ObservableObject`, `@ObservedObject`, or `@Published` — always prefer `@Observable`
- Use `@Binding` to pass mutable state down to child views
- Use `.environment(_:)` to inject `@Observable` objects into the environment

```swift
// ✅ Modern
@MainActor
@Observable
final class AppModel {
    var isLoggedIn = false
}

struct MyApp: App {
    @State private var appModel = AppModel()
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(appModel)
        }
    }
}

// ❌ Old — do not use
class OldModel: ObservableObject {
    @Published var isLoggedIn = false
}
```

### View Modifiers & APIs

- Always use `foregroundStyle()` instead of `foregroundColor()`
- Always use `clipShape(.rect(cornerRadius:))` instead of `.cornerRadius()`
- Always use the `Tab` API instead of `tabItem()`
- **Never** use `onChange()` in its 1-parameter variant — use the 2-parameter `(old, new)` or 0-parameter variant
- **Never** use `onTapGesture()` unless you need tap location or count — use `Button` instead
- When hiding scroll indicators, use `.scrollIndicators(.hidden)` not `showsIndicators: false`
- Don’t apply `fontWeight()` just for bold — use `.bold()` directly
- Avoid `GeometryReader` when `containerRelativeFrame()` or `visualEffect()` would work
- Avoid `AnyView` unless absolutely required
- Use `ImageRenderer` instead of `UIGraphicsImageRenderer` in SwiftUI contexts

### Performance

- Use `LazyVStack`/`LazyHStack` for large collections
- For `ForEach` on enumerated sequences: `ForEach(x.enumerated(), id: \.element.id)` — do **not** wrap in `Array(...)`
- Avoid hard-coded padding/spacing values unless necessary

### Data

- Use `SwiftData` for persistence
- **Never** use `@Attribute(.unique)` — model properties must have default values or be optional
- All relationships must be marked optional

-----

## Code Style

- Use `nonisolated` on methods that don’t need actor isolation to improve performance
- Avoid force unwraps (`!`) and force try (`try!`) unless the error is truly unrecoverable
- Use `localizedStandardContains()` for user-input text filtering (not `contains()`)
- Prefer Swift-native alternatives to Foundation: e.g. `replacing("a", with: "b")` over `replacingOccurrences(of:with:)`
- Use modern Foundation API: `URL.documentsDirectory`, `url.appending(path:)`
- Break types into separate files — no multiple structs/classes/enums in one file
- Use consistent feature-based folder structure
- Add documentation comments (`///`) for public-facing APIs

-----

## Testing

- Write unit tests for core application logic
- Only write UI tests if unit tests are not possible
- Use Swift Testing framework (`@Test`, `#expect`) over XCTest where possible

-----

## Security

- Never include API keys or secrets in the repository
- Use the Keychain for sensitive credential storage

-----

## Resources

- [twostraws/SwiftAgents](https://github.com/twostraws/SwiftAgents) — AGENTS.md source
- [AvdLee/Swift-Concurrency-Agent-Skill](https://github.com/AvdLee/Swift-Concurrency-Agent-Skill) — deep Swift Concurrency skill
- [steipete/agent-rules](https://github.com/steipete/agent-rules) — additional modern Swift rules
