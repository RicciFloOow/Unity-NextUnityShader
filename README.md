# Next Unity Shader

在 Unity 中，若要使用 Shader Model 6.0 (SM6.0) 的特性，仅能通过 `#pragma use_dxc` 强制使用 DXC 编译。然而，尽管 Unity 2022.3+ 使用的 DXC 1.6+ 理论上支持至 SM6.6，但 `UnityShaderCompiler` 传递给 DXC 的参数硬编码了目标版本为 6.0。这导致除 DXR 外（支持情况依 Unity 版本而定），SM6.0 以上的特性均无法正常编译（实测unity6.3LTS的依然如此）。

本项目的核心原理是：利用 MinHook 劫持 Unity 向 DXC 提交的 HLSL 代码与编译参数。通过白名单识别出允许的特性后，自动将编译目标版本动态提升至该特性所需的 Shader Model 版本（如 SM6.6）。

---

## 使用说明

### 步骤

1. 进入目标 Unity 编辑器安装目录下的 `Editor\Data\Tools` 文件夹，将自带的 `dxcompiler.dll` 重命名为 `dxcompiler_orig.dll`。
2. 将本项目提供的 `Release\x64\dxcompiler.dll` 拷贝至上述同级目录中。
3. 若需使用 SM6.6 的原子操作，请使用 `#include` 引入或参考项目中提供的 `SampleNUS\Assets\Shader\NUSLib.hlsl`。

### 白名单特性

* **SM 6.2:** 原生16位类型的支持: SM6.2及以上的白名单特性被识别时自动支持，或者使用`Require16bitType()`的空方法来自动识别。
* **SM 6.4:** [Packed Dot Product Intrinsics](https://github.com/microsoft/DirectXShaderCompiler/wiki/Shader-Model-6.4)
* **SM 6.6:**
    * 新增的 [Wave-Intrinsics](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_ShaderModel6_5.html#new-wave-intrinsics)
    * [Int64 与 Float 的原子操作](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html)
    * [Pack 与 Unpack-Intrinsics](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Pack_Unpack_Intrinsics.html)
* **SM 6.7:** [Quad 特性](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_7_QuadAny_QuadAll.html)

> 若需扩展更多特性，请自行在 `shader_inspect.cpp` 的 `BuildBuiltinWhitelist()` 方法中添加。

---

## 注意事项

### SM6.6 原子操作

由于 SM5.0 中 `Int64/Uint64/Float` 的部分原子操作属于重载方法，无法仅凭方法名区分应使用 SM5.0 还是 SM6.6。因此，我们引入了一个特殊的空方法 `NUS_Require_SM66_Atomics()` 作为介入入口。具体实现请参考 `NUSLib.hlsl` 及示例文件 `SampleCS.compute`。

### 在普通光栅化 Shader 中使用 SM6.+ 特性

在 Compute Shader 中使用白名单特性时，即使目标平台是 DX11，底层也会被提升至 SM6.+ 编译且不显式报错（只需在 C# 端控制好变体即可）。但在普通光栅化 Shader 中，即使强制使用 `#pragma use_dxc`，Unity 在底层仍会强制多跑一遍 DX11（即 SM5.0）的编译流程，遇到 SM6.+ 代码时会直接报错。

为此，我们提供了一个专用宏 `UNITY_SM6_SELECT` 来规避此问题：

```hlsl
UNITY_SM6_SELECT
(
	fallback的值,
	目标的——使用SM6.+的代码块
)
```

通过该宏标记后，我们的 Proxy DLL 会在预处理阶段自动识别：在编译低版本目标时采用 fallback 值，编译高版本目标时采用 SM6.+ 代码，从而安全绕过报错逻辑。



## 第三方库鸣谢

本项目使用了如下第三方库：

*   **[MinHook](https://github.com/TsudaKageyu/minhook)** - The Minimalistic API Hooking Library for x64/x86. Licensed under the [BSD 2-Clause License](https://github.com/TsudaKageyu/minhook/blob/master/LICENSE.txt).