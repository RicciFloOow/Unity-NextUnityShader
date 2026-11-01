# Next Unity Shader

在 Unity 中，若要使用 Shader Model 6.0 (SM6.0) 的特性，仅能通过 `#pragma use_dxc` 强制使用 DXC 编译。然而，尽管 Unity 2022.3+ 使用的 DXC 1.6+ 理论上支持至 SM6.6，但 `UnityShaderCompiler` 传递给 DXC 的参数硬编码了目标版本为 6.0。这导致除 DXR 外（支持情况依 Unity 版本而定），SM6.0 以上的特性均无法正常编译（实测unity6.3LTS的依然如此）。

本项目的核心原理是：利用 MinHook 劫持 Unity 向 DXC 提交的 HLSL 代码与编译参数。通过白名单识别出允许的特性后，自动将编译目标版本动态提升至该特性所需的 Shader Model 版本（如 SM6.6）。

---

## 使用说明

### 步骤

1. 进入目标 Unity 编辑器安装目录下的 `Editor\Data\Tools` 文件夹，将自带的 `dxcompiler.dll` 重命名为 `dxcompiler_orig.dll`。
2. 将本项目提供的 `Release\x64\dxcompiler.dll` 拷贝至上述同级目录中。
3. 若需使用 SM6.6 的原子操作，请使用 `#include` 引入或参考项目中提供的 `SampleNUS\Assets\Shader\NUSLib_66.hlsl`。
4. NUSLib 按 SM 版本拆分（`NUSLib_62/66/68/69/610.hlsl`），高版本 include 低版本，按需引入即可——白名单对源码做的是子串匹配，include 进来的标记定义也会命中，因此引入的版本决定了编译目标可能被提升到的上限。

### 白名单特性

* **SM 6.2:** 原生16位类型的支持: SM6.2及以上的白名单特性被识别时自动支持，或者使用`Require16bitType()`的空方法来自动识别。
* **SM 6.4:** [Packed Dot Product Intrinsics](https://github.com/microsoft/DirectXShaderCompiler/wiki/Shader-Model-6.4)
* **SM 6.5**: 新增的 [Wave-Intrinsics](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_ShaderModel6_5.html#new-wave-intrinsics)
* **SM 6.6:**
    * [CS中Quad*操作的支持](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Derivatives.html)
    * [Int64 与 Float 的原子操作](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html)
    * [Pack 与 Unpack-Intrinsics](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Pack_Unpack_Intrinsics.html)
    * [Bindless]([HLSL Dynamic Resources | DirectX-Specs](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html))
* **SM 6.7:** [Quad 特性](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_7_QuadAny_QuadAll.html)
* **SM 6.8:** [Extended Command Info](https://microsoft.github.io/hlsl-specs/proposals/0015-extended-command-info/)（`SV_StartVertexLocation` / `SV_StartInstanceLocation` 等系统语义）
* **SM 6.9:** [Long Vector 长向量类型](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0026-hlsl-long-vector-type.md)
* **SM 6.10（preview）:**
    * [Group Wave Index](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0048-group-wave-index.md)（`GetGroupWaveIndex` / `GetGroupWaveCount`）
    * [Variable Groupshared Memory](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0049-variable-groupshared-memory.md)（`GroupSharedLimit`）
    * [LinAlg Matrix](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0035-linalg-matrix.md)

> 若需扩展更多特性，请自行在 `shader_inspect.cpp` 的 `BuildBuiltinWhitelist()` 方法中添加。

---

## 注意事项

### SM6.6 Quad Shuffle

`QuadReadLaneAt` 等操作在像素着色器中可直接使用( SM6.0 下)。在CS等需要更高目标版本的场景中，请使用 `NUS_Require_SM66_QuadShuffle()` 作为显式标记。

### SM6.6 原子操作

由于 SM5.0 中 `Int64/Uint64/Float` 的部分原子操作属于重载方法，无法仅凭方法名区分应使用 SM5.0 还是 SM6.6。因此，我们引入了一个特殊的空方法 `NUS_Require_SM66_Atomics()` 作为介入入口。具体实现请参考 `NUSLib_66.hlsl` 及示例文件 `SampleCS.compute`。

### SM6.6 Bindless

由于`ResourceDescriptorHeap`这种并不是方法形式的，所以为了方便，我还是用`NUS_Require_SM66_Bindless()`这种空方法作为标记需要SM6.6编译的入口。注意，这个dll只是提供了编译，在unity中具体使用Bindless需要额外的插件，可以参考我的另一个示例项目：[RicciFloOow/Unity-BindlessSample](https://github.com/RicciFloOow/Unity-BindlessSample)

### SM6.8 Extended Command Info

`SV_StartVertexLocation` 这类是系统语义而不是方法形式，无法自动识别，所以还是用空方法 `Require_SM68_ExtendedCommandInfo()` 作为显式标记。

### SM6.9 Long Vector

同理，长向量是类型而不是方法，使用空方法 `Require_SM69_LongVector()` 作为显式标记。

### SM6.10（preview）特性

`GetGroupWaveIndex` / `GetGroupWaveCount` / `GroupSharedLimit` 是方法形式，可直接识别；LinAlg Matrix 是类型形式，使用空方法 `Require_SM610_LinalgMatrix()` 作为显式标记。上述空方法已在 `NUSLib_610.hlsl` 中提供，`#include` 引入即可使用。

注意，SM6.10 属于 preview 特性，需要同时满足：

1. **编译侧**：真身 `dxcompiler_orig.dll` 需为 DXC 1.10（unity 6.6 自带的即是，6.3 的为 1.7）。代理会按真身 dxc 版本自动过滤白名单，版本不够的特性不会被误提升；
2. **运行时侧**：preview Shader Model 必须经运行时放行，需配合 [Unity-EditorPreviewAgilityExt](https://github.com/RicciFloOow/Unity-EditorPreviewAgilityExt) 将编辑器的 Agility SDK 切换到 1.722+（同时要求开启 Windows 开发者模式），否则编译通过也会在 PSO 创建时失败（`E_INVALIDARG`）；
3. **CS 的 dx11 flavor 问题**：Unity 在分发编译时会自动处理 dx11 flavor——目标为 SM6.+ 的编译不会真正走 FXC，而是将 dx12 编译好的结果作为壳传回 dx11（因为实际不会在 dx11 下运行），这套处理是 Unity 内部行为，代理无法干预。因此 SM6.6 及以下特性均不会报错；但 SM6.7+ 的新特性不在 Unity 这套处理范围内，会真正落到 FXC 编译并报 `undeclared identifier (on dx11)`。该报错只影响 dx11 flavor，不影响 dx12 的实际运行。后续会出 FXC 代理（D3DCompiler_47.dll proxy）来解决；现阶段尝鲜可将项目 Graphics API 移除 DX11，或暂时忍受该报错。

### 在普通光栅化 Shader 中使用 SM6.+ 特性

在 Compute Shader 中使用白名单特性时，即使目标平台是 DX11，底层也会被提升至 SM6.+ 编译。Unity 在分发编译时会自动处理 dx11 flavor（SM6.+ 的目标不会真正走 FXC 编译，而是以 dx12 的结果作为壳传回），因此 SM6.6 及以下特性不显式报错；SM6.7+ 的新特性不在此处理范围内，会在 dx11 flavor 下报错（详见「SM6.10（preview）特性」第 3 点），只需在 C# 端控制好变体即可。但在普通光栅化 Shader 中，即使强制使用 `#pragma use_dxc`，Unity 在底层仍会强制多跑一遍 DX11（即 SM5.0）的编译流程，遇到 SM6.+ 代码时会直接报错。

为此，我们提供了一个专用宏 `UNITY_SM6_SELECT` 来规避此问题：

```hlsl
UNITY_SM6_SELECT
(
	fallback的值,
	目标的——使用SM6.+的代码块
)
```

通过该宏标记后，我们的 Proxy DLL 会在预处理阶段自动识别：在编译低版本目标时采用 fallback 值，编译高版本目标时采用 SM6.+ 代码，从而安全绕过报错逻辑。

### 旧版本unity注意

对于unity 2022.3.+这类"早期"版本，unity所用的dxc还不支持HLSL 2021的特性，因此需要修改`REQUIRE_HLSL2021`为0来编译(unity 6.3所用的dxc版本实测支持了)



## 第三方库鸣谢

本项目使用了如下第三方库：

*   **[MinHook](https://github.com/TsudaKageyu/minhook)** - The Minimalistic API Hooking Library for x64/x86. Licensed under the [BSD 2-Clause License](https://github.com/TsudaKageyu/minhook/blob/master/LICENSE.txt).