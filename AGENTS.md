# C++编码规范

## 命名规范

所有 C++ 标识符遵循以下命名约定：

| Category | Case | Prefix/Suffix | Example |
|---|---|---|---|
| Classes | PascalCase | — | `HttpClient`, `FileManager` |
| Structs | PascalCase | — | `Point`, `BufferView` |
| Functions | snake_case | — | `parse_input`, `get_connection` |
| Methods | snake_case | — | `start_service`, `is_ready` |
| Variables | snake_case | — | `buffer`, `max_count` |
| Local variables | snake_case | — | `result`, `temp_buffer` |
| Member variables | snake_case | trailing `_` | `count_`, `data_ptr_` |
| Constants | PascalCase | prefix `k` | `kMaxSize`, `kDefaultTimeout` |
| Template parameters | PascalCase | — | `T`, `Allocator`, `KeyType` |

## 注释规范

- 注释**必须使用英文**
- 极其克制，非必要不注释
- 注释只用于「代码本身无法表达的信息」。凡是可以从代码中直接读出的内容，一律不写注释

## 代码复用

- 避免重复代码，发现相同或相似的逻辑时提取功能函数
- 优先使用标准库，不重复造轮子

## 现代特性

- 优先使用 C++17/20/23/26 现代特性
- 优先编译期计算：`constexpr`、`constinit`、`consteval`、模板元编程
- 灵活使用标准属性（attribute）：
  - `[[likely]]` / `[[unlikely]]` — 分支预测提示，标注大概率/小概率执行的分支
  - `[[nodiscard]]` — 返回值不应被丢弃
  - `[[maybe_unused]]` — 抑制未使用警告
  - `[[assume]]` (C++23) / `__builtin_assume` — 编译器优化假设

```cpp
if (result.has_value()) [[likely]] {
  return *result;
} else [[unlikely]] {
  return error_code::kUnknown;
}

[[nodiscard]] auto compute_value() -> int;

void helper([[maybe_unused]] int debug_flag) { /* ... */ }
```

## 性能优化

- 性能关键处使用缓存友好的数据导向设计（Data-Oriented Design），必要时用 `alignas` 对齐缓存行
- 避免无谓的拷贝：能用引用/移动语义的地方不用值传递

## 模块化

- 代码按功能模块拆分，使用命名空间隔离
- `main` 函数保持简洁清晰，只做模块组装和数据流编排

# Build

```sh
./build.sh            # Debug build (default)
./build.sh Release    # Release build
```

# Test

```sh
./run_tests.sh
```
