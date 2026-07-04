internal static class DxgiFormats
{
    public const uint R16G16B16A16Float = 10;
    public const uint R10G10B10A2Unorm = 24;
    public const uint R8G8B8A8Unorm = 28;
    public const uint R8G8B8A8UnormSrgb = 29;
    public const uint B8G8R8A8Unorm = 87;
    public const uint B8G8R8A8UnormSrgb = 91;

    public static string Name(uint format) => format switch
    {
        R16G16B16A16Float => "R16G16B16A16_FLOAT",
        R10G10B10A2Unorm => "R10G10B10A2_UNORM",
        R8G8B8A8Unorm => "R8G8B8A8_UNORM",
        R8G8B8A8UnormSrgb => "R8G8B8A8_UNORM_SRGB",
        B8G8R8A8Unorm => "B8G8R8A8_UNORM",
        B8G8R8A8UnormSrgb => "B8G8R8A8_UNORM_SRGB",
        _ => $"FORMAT_{format}"
    };
}
