# RT Contact Shadows

This feature implements hardware-accelerated contact shadows using DirectX 12 Ultimate raytracing capabilities.

## Requirements

- DirectX 12 Ultimate compatible GPU (RTX 20-series+, RDNA2+)
- Windows 10 version 2004+ or Windows 11
- Hardware raytracing support

## Description

RT Contact Shadows uses the RT cores on modern GPUs to provide accurate contact shadows between objects and surfaces. Unlike traditional shadow mapping or screen-space techniques, hardware raytracing can accurately determine occlusion even for thin geometry and complex scenes.

## Features

- Hardware-accelerated raytracing using DX12 Ultimate
- Automatic capability detection and fallback
- Configurable intensity and maximum distance
- Integration with existing Skyrim lighting system
- Performance scaling based on ray count

## Settings

- **Enable**: Toggles the feature on/off (auto-disabled if RT not supported)
- **Intensity**: Controls the strength of contact shadows (0.0 = no shadows, 2.0 = maximum)
- **Max Distance**: Maximum ray distance for contact shadow detection
- **Max Steps**: Ray marching quality control (affects performance)

## Technical Details

The implementation uses:

- DX12 raytracing pipeline with raygen, miss, and anyhit shaders
- Scene acceleration structures (BLAS/TLAS) built from game geometry
- Integration with existing G-buffer (depth, normals)
- Efficient early ray termination for performance
- DX11/DX12 interop through existing proxy infrastructure

## Performance Notes

RT Contact Shadows can be performance-intensive depending on:

- Screen resolution
- Scene complexity (geometry density)
- Max distance and step count settings
- GPU raytracing performance

Recommended to start with lower settings and adjust based on performance.
