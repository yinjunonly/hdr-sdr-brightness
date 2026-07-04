using System;

internal static class PixelMath
{
    public static byte ToByte(float value) => (byte)Math.Clamp((int)(value * 255.0f + 0.5f), 0, 255);
}
