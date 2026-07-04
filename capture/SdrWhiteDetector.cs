using System;
using System.Runtime.InteropServices;

internal static class SdrWhiteDetector
{
    private const uint QdcOnlyActivePaths = 0x00000002;
    private const uint DisplayConfigGetSdrWhiteLevel = 11;
    private const uint ErrorSuccess = 0;
    private const uint ErrorInsufficientBuffer = 122;

    public static float DetectCurrent(float fallback)
    {
        uint status = GetDisplayConfigBufferSizes(QdcOnlyActivePaths, out uint pathCount, out uint modeCount);
        if (status != ErrorSuccess || pathCount == 0) return fallback;

        for (int attempt = 0; attempt < 4; attempt++)
        {
            DisplayConfigPathInfo[] paths = new DisplayConfigPathInfo[pathCount];
            DisplayConfigModeInfo[] modes = new DisplayConfigModeInfo[Math.Max(1, modeCount)];
            uint queryPathCount = pathCount;
            uint queryModeCount = modeCount;
            status = QueryDisplayConfig(QdcOnlyActivePaths, ref queryPathCount, paths, ref queryModeCount, modes, nint.Zero);
            if (status == ErrorInsufficientBuffer)
            {
                status = GetDisplayConfigBufferSizes(QdcOnlyActivePaths, out pathCount, out modeCount);
                if (status != ErrorSuccess) return fallback;
                continue;
            }
            if (status != ErrorSuccess) return fallback;

            for (int i = 0; i < queryPathCount; i++)
            {
                DisplayConfigSdrWhiteLevel info = new()
                {
                    Header = new DisplayConfigDeviceInfoHeader
                    {
                        Type = DisplayConfigGetSdrWhiteLevel,
                        Size = (uint)Marshal.SizeOf<DisplayConfigSdrWhiteLevel>(),
                        AdapterId = paths[i].TargetInfo.AdapterId,
                        Id = paths[i].TargetInfo.Id
                    }
                };

                if (DisplayConfigGetDeviceInfo(ref info) == ErrorSuccess && info.SdrWhiteLevel > 0)
                {
                    float value = Math.Clamp(info.SdrWhiteLevel / 1000.0f, 0.1f, 10.0f);
                    Console.WriteLine($"Detected SDR white level: {info.SdrWhiteLevel} ({value:F2} scRGB)");
                    return value;
                }
            }
            break;
        }

        return fallback;
    }

    [DllImport("user32.dll")]
    private static extern uint GetDisplayConfigBufferSizes(uint flags, out uint numPathArrayElements, out uint numModeInfoArrayElements);

    [DllImport("user32.dll")]
    private static extern uint QueryDisplayConfig(
        uint flags,
        ref uint numPathArrayElements,
        [Out] DisplayConfigPathInfo[] pathInfoArray,
        ref uint numModeInfoArrayElements,
        [Out] DisplayConfigModeInfo[] modeInfoArray,
        nint currentTopologyId);

    [DllImport("user32.dll", EntryPoint = "DisplayConfigGetDeviceInfo")]
    private static extern uint DisplayConfigGetDeviceInfo(ref DisplayConfigSdrWhiteLevel requestPacket);

    [StructLayout(LayoutKind.Sequential)]
    private struct Luid
    {
        public uint LowPart;
        public int HighPart;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigPathSourceInfo
    {
        public Luid AdapterId;
        public uint Id;
        public uint ModeInfoIdx;
        public uint StatusFlags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigRational
    {
        public uint Numerator;
        public uint Denominator;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigPathTargetInfo
    {
        public Luid AdapterId;
        public uint Id;
        public uint ModeInfoIdx;
        public uint OutputTechnology;
        public uint Rotation;
        public uint Scaling;
        public DisplayConfigRational RefreshRate;
        public uint ScanLineOrdering;
        [MarshalAs(UnmanagedType.Bool)] public bool TargetAvailable;
        public uint StatusFlags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigPathInfo
    {
        public DisplayConfigPathSourceInfo SourceInfo;
        public DisplayConfigPathTargetInfo TargetInfo;
        public uint Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private unsafe struct DisplayConfigModeInfo
    {
        public uint InfoType;
        public uint Id;
        public Luid AdapterId;
        public fixed byte ModeInfo[48];
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigDeviceInfoHeader
    {
        public uint Type;
        public uint Size;
        public Luid AdapterId;
        public uint Id;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DisplayConfigSdrWhiteLevel
    {
        public DisplayConfigDeviceInfoHeader Header;
        public uint SdrWhiteLevel;
    }
}
