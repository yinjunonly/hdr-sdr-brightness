using System;
using System.Threading.Tasks;

internal sealed record ReadbackResult(uint Width, uint Height, uint Format, PixelStats Stats, byte[] Bgra)
{
    public void PrintStats()
    {
        Console.WriteLine($"Sample R min/avg/max: {Stats.MinR:F6} / {Stats.AvgR:F6} / {Stats.MaxR:F6}");
        Console.WriteLine($"Sample G min/avg/max: {Stats.MinG:F6} / {Stats.AvgG:F6} / {Stats.MaxG:F6}");
        Console.WriteLine($"Sample B min/avg/max: {Stats.MinB:F6} / {Stats.AvgB:F6} / {Stats.MaxB:F6}");
        if (Format == DxgiFormats.R16G16B16A16Float)
        {
            Console.WriteLine(Stats.MaxR > 1.0 || Stats.MaxG > 1.0 || Stats.MaxB > 1.0
                ? "HDR signal: sampled float values exceed 1.0."
                : "No sampled HDR headroom: float values did not exceed 1.0.");
        }
    }
}

internal readonly record struct PixelStats(
    double MinR,
    double AvgR,
    double MaxR,
    double MinG,
    double AvgG,
    double MaxG,
    double MinB,
    double AvgB,
    double MaxB)
{
    public static unsafe PixelStats FromMapped(nint data, uint rowPitch, uint format, uint width, uint height)
    {
        double minR = double.MaxValue, minG = double.MaxValue, minB = double.MaxValue;
        double maxR = double.MinValue, maxG = double.MinValue, maxB = double.MinValue;
        double sumR = 0.0, sumG = 0.0, sumB = 0.0;
        ulong count = 0;
        uint stepX = Math.Max(1, width / 256);
        uint stepY = Math.Max(1, height / 144);

        for (uint y = 0; y < height; y += stepY)
        {
            byte* row = (byte*)data + rowPitch * y;
            for (uint x = 0; x < width; x += stepX)
            {
                (double r, double g, double b) = ToneMapper.ReadRgb(row, x, format);
                minR = Math.Min(minR, r);
                minG = Math.Min(minG, g);
                minB = Math.Min(minB, b);
                maxR = Math.Max(maxR, r);
                maxG = Math.Max(maxG, g);
                maxB = Math.Max(maxB, b);
                sumR += r;
                sumG += g;
                sumB += b;
                count++;
            }
        }

        if (count == 0) count = 1;
        return new PixelStats(minR, sumR / count, maxR, minG, sumG / count, maxG, minB, sumB / count, maxB);
    }
}

internal readonly record struct ToneMapOptions(string Mode, float SdrWhite, float SdrOutputWhite, float HdrKnee, float HdrShoulder, float Exposure)
{
    public static ToneMapOptions FromArgs(string[] args)
    {
        string mode = CaptureArgs.Value(args, "--tone-map") ?? "desktop";
        string? sdrWhiteArg = CaptureArgs.Value(args, "--sdr-white");
        float detectedSdrWhite = SdrWhiteDetector.DetectCurrent(3.0f);
        float sdrWhite = !string.IsNullOrWhiteSpace(sdrWhiteArg)
            ? Math.Clamp(CaptureArgs.Float(args, "--sdr-white", detectedSdrWhite), 0.1f, 10.0f)
            : detectedSdrWhite;
        float sdrOutputWhite = Math.Clamp(CaptureArgs.Float(args, "--sdr-output-white", 1.0f), 0.5f, 1.0f);
        float hdrKnee = Math.Clamp(CaptureArgs.Float(args, "--hdr-knee", 0.55f), 0.1f, 1.0f);
        float hdrShoulder = Math.Clamp(CaptureArgs.Float(args, "--hdr-shoulder", 5.0f), 0.1f, 10.0f);
        float exposure = Math.Clamp(CaptureArgs.Float(args, "--exposure", 0.75f), 0.1f, 2.0f);
        Console.WriteLine($"Tone map: {mode}, SDR white {sdrWhite:F2}, SDR output white {sdrOutputWhite:F2}, HDR knee {hdrKnee:F2}, HDR shoulder {hdrShoulder:F2}, exposure {exposure:F2}");
        return new ToneMapOptions(mode, sdrWhite, sdrOutputWhite, hdrKnee, hdrShoulder, exposure);
    }
}

internal static class ToneMapper
{
    private static readonly float[] HalfFloatLookup = BuildHalfFloatLookup();

    public static unsafe byte[] ConvertToBgra(nint data, uint rowPitch, uint format, uint width, uint height, ToneMapOptions toneMap)
    {
        byte[] output = new byte[checked((int)(width * height * 4))];
        if (format == DxgiFormats.R16G16B16A16Float)
        {
            ConvertR16FloatToBgra(data, rowPitch, width, height, toneMap, output);
            return output;
        }

        fixed (byte* outputPtr = output)
        {
            for (uint y = 0; y < height; y++)
            {
                byte* src = (byte*)data + rowPitch * y;
                byte* dst = outputPtr + width * y * 4;
                for (uint x = 0; x < width; x++)
                {
                    byte* px = dst + x * 4;
                    switch (format)
                    {
                        case DxgiFormats.B8G8R8A8Unorm:
                        case DxgiFormats.B8G8R8A8UnormSrgb:
                        {
                            byte* s = src + x * 4;
                            px[0] = s[0];
                            px[1] = s[1];
                            px[2] = s[2];
                            px[3] = 255;
                            break;
                        }
                        case DxgiFormats.R8G8B8A8Unorm:
                        case DxgiFormats.R8G8B8A8UnormSrgb:
                        {
                            byte* s = src + x * 4;
                            px[0] = s[2];
                            px[1] = s[1];
                            px[2] = s[0];
                            px[3] = 255;
                            break;
                        }
                        case DxgiFormats.R10G10B10A2Unorm:
                        {
                            uint packed = ((uint*)src)[x];
                            px[2] = PixelMath.ToByte((packed & 0x3ff) / 1023.0f);
                            px[1] = PixelMath.ToByte(((packed >> 10) & 0x3ff) / 1023.0f);
                            px[0] = PixelMath.ToByte(((packed >> 20) & 0x3ff) / 1023.0f);
                            px[3] = 255;
                            break;
                        }
                        default:
                            px[0] = 0;
                            px[1] = 0;
                            px[2] = 0;
                            px[3] = 255;
                            break;
                    }
                }
            }
        }
        return output;
    }

    public static unsafe (double R, double G, double B) ReadRgb(byte* row, uint x, uint format)
    {
        return format switch
        {
            DxgiFormats.B8G8R8A8Unorm or DxgiFormats.B8G8R8A8UnormSrgb =>
                (row[x * 4 + 2] / 255.0, row[x * 4 + 1] / 255.0, row[x * 4] / 255.0),
            DxgiFormats.R8G8B8A8Unorm or DxgiFormats.R8G8B8A8UnormSrgb =>
                (row[x * 4] / 255.0, row[x * 4 + 1] / 255.0, row[x * 4 + 2] / 255.0),
            DxgiFormats.R10G10B10A2Unorm => ReadR10G10B10((uint*)row, x),
            DxgiFormats.R16G16B16A16Float => ReadFloat16(row, x),
            _ => (0.0, 0.0, 0.0)
        };
    }

    private static unsafe void ConvertR16FloatToBgra(nint data, uint rowPitch, uint width, uint height, ToneMapOptions toneMap, byte[] output)
    {
        ToneMapLookup lookup = new(toneMap);
        int w = checked((int)width);
        int h = checked((int)height);
        fixed (byte* outputPtr = output)
        {
            nint sourceBase = data;
            nint destinationBase = (nint)outputPtr;
            Parallel.For(0, h, y =>
            {
                byte* src = (byte*)sourceBase + rowPitch * (uint)y;
                byte* dst = (byte*)destinationBase + (nuint)(w * y * 4);
                for (int x = 0; x < w; x++)
                {
                    ushort* s = (ushort*)(src + x * 8);
                    byte* px = dst + x * 4;
                    ToneMapPixel(HalfToSingle(s[0]), HalfToSingle(s[1]), HalfToSingle(s[2]), lookup, px);
                    px[3] = 255;
                }
            });
        }
    }

    private static unsafe (double R, double G, double B) ReadR10G10B10(uint* row, uint x)
    {
        uint packed = row[x];
        return ((packed & 0x3ff) / 1023.0, ((packed >> 10) & 0x3ff) / 1023.0, ((packed >> 20) & 0x3ff) / 1023.0);
    }

    private static unsafe (double R, double G, double B) ReadFloat16(byte* row, uint x)
    {
        ushort* pixel = (ushort*)(row + x * 8);
        return (HalfToSingle(pixel[0]), HalfToSingle(pixel[1]), HalfToSingle(pixel[2]));
    }

    private static float[] BuildHalfFloatLookup()
    {
        float[] table = new float[ushort.MaxValue + 1];
        for (int i = 0; i < table.Length; i++)
        {
            table[i] = (float)BitConverter.UInt16BitsToHalf((ushort)i);
        }
        return table;
    }

    private static float HalfToSingle(ushort value) => HalfFloatLookup[value];

    private static unsafe void ToneMapPixel(float r, float g, float b, ToneMapLookup lookup, byte* bgra)
    {
        r = Math.Max(0.0f, r * lookup.Exposure);
        g = Math.Max(0.0f, g * lookup.Exposure);
        b = Math.Max(0.0f, b * lookup.Exposure);
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        if (luminance <= 0.000001f)
        {
            bgra[0] = 0;
            bgra[1] = 0;
            bgra[2] = 0;
            return;
        }

        float mappedLuminance = lookup.MapLuminance(luminance);
        float scale = mappedLuminance / luminance;
        bgra[2] = lookup.ToSrgbByte(r * scale);
        bgra[1] = lookup.ToSrgbByte(g * scale);
        bgra[0] = lookup.ToSrgbByte(b * scale);
    }

    private static float ToneMapDesktop(float value, ToneMapOptions options)
    {
        float kneeStart = Math.Clamp(options.SdrWhite * options.HdrKnee, 0.0f, options.SdrWhite);
        float kneeOutput = kneeStart / options.SdrWhite * options.SdrOutputWhite;
        if (value <= kneeStart)
        {
            return Math.Clamp(value / options.SdrWhite * options.SdrOutputWhite, 0.0f, 1.0f);
        }

        float over = (value - kneeStart) / options.HdrShoulder;
        float highlight = 1.0f - MathF.Exp(-Math.Max(0.0f, over));
        return Math.Clamp(kneeOutput + (1.0f - kneeOutput) * highlight, 0.0f, 1.0f);
    }

    private static float LinearToSrgb(float value)
    {
        value = Math.Clamp(value, 0.0f, 1.0f);
        return value <= 0.0031308f
            ? value * 12.92f
            : 1.055f * MathF.Pow(value, 1.0f / 2.4f) - 0.055f;
    }

    

    private sealed class ToneMapLookup
    {
        private const int SrgbTableSize = 4096;
        private const int LuminanceTableSize = 8192;
        private const float MaxLuminance = 16.0f;
        private readonly byte[] srgb = new byte[SrgbTableSize];
        private readonly float[] mappedLuminance = new float[LuminanceTableSize];

        public float Exposure { get; }

        public ToneMapLookup(ToneMapOptions options)
        {
            Exposure = options.Exposure;
            for (int i = 0; i < srgb.Length; i++)
            {
                srgb[i] = PixelMath.ToByte(LinearToSrgb(i / (float)(srgb.Length - 1)));
            }

            bool reinhard = string.Equals(options.Mode, "reinhard", StringComparison.OrdinalIgnoreCase);
            for (int i = 0; i < mappedLuminance.Length; i++)
            {
                float value = i * MaxLuminance / (mappedLuminance.Length - 1);
                mappedLuminance[i] = reinhard
                    ? value / (1.0f + value)
                    : ToneMapDesktop(value, options);
            }
        }

        public byte ToSrgbByte(float value)
        {
            if (value <= 0.0f) return 0;
            if (value >= 1.0f) return 255;
            int index = (int)(value * (SrgbTableSize - 1) + 0.5f);
            return srgb[index];
        }

        public float MapLuminance(float luminance)
        {
            if (luminance <= 0.0f) return 0.0f;
            if (luminance >= MaxLuminance) return mappedLuminance[^1];
            int index = (int)(luminance * (LuminanceTableSize - 1) / MaxLuminance + 0.5f);
            return mappedLuminance[index];
        }
    }
}

