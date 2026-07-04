using System.Drawing;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
        private System.Drawing.Rectangle CurrentEditSelection()
        {
            int left = Math.Clamp(Math.Min(editStart.X, editCurrent.X), 0, ClientSize.Width);
            int top = Math.Clamp(Math.Min(editStart.Y, editCurrent.Y), 0, ClientSize.Height);
            int right = Math.Clamp(Math.Max(editStart.X, editCurrent.X), 0, ClientSize.Width);
            int bottom = Math.Clamp(Math.Max(editStart.Y, editCurrent.Y), 0, ClientSize.Height);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private static System.Drawing.Point ClampPointToRectangle(System.Drawing.Point point, System.Drawing.Rectangle rect)
        {
            if (rect.Width <= 0 || rect.Height <= 0) return point;
            return new System.Drawing.Point(
                Math.Clamp(point.X, rect.Left, rect.Right - 1),
                Math.Clamp(point.Y, rect.Top, rect.Bottom - 1));
        }

        private static System.Drawing.Rectangle InsetRectangle(System.Drawing.Rectangle rect, int inset)
        {
            if (rect.Width <= inset * 2 || rect.Height <= inset * 2) return rect;
            return System.Drawing.Rectangle.Inflate(rect, -inset, -inset);
        }

        private static System.Drawing.Rectangle InflateRectangle(System.Drawing.Rectangle rect, int amount)
        {
            return System.Drawing.Rectangle.Inflate(rect, amount, amount);
        }

        private System.Drawing.Rectangle ImageToClient(System.Drawing.Rectangle rect)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / Math.Max(1.0, (double)preview.Width);
            double scaleY = imageBounds.Height / Math.Max(1.0, (double)preview.Height);
            int x = imageBounds.X + (int)Math.Round(rect.X * scaleX);
            int y = imageBounds.Y + (int)Math.Round(rect.Y * scaleY);
            int right = imageBounds.X + (int)Math.Round(rect.Right * scaleX);
            int bottom = imageBounds.Y + (int)Math.Round(rect.Bottom * scaleY);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Point ClientPointToImage(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = preview.Width / Math.Max(1.0, (double)imageBounds.Width);
            double scaleY = preview.Height / Math.Max(1.0, (double)imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((point.X - imageBounds.X) * scaleX), 0, preview.Width - 1);
            int y = Math.Clamp((int)Math.Round((point.Y - imageBounds.Y) * scaleY), 0, preview.Height - 1);
            return new System.Drawing.Point(x, y);
        }

        private System.Drawing.Point ImagePointToClient(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / Math.Max(1.0, (double)preview.Width);
            double scaleY = imageBounds.Height / Math.Max(1.0, (double)preview.Height);
            int x = imageBounds.X + (int)Math.Round(point.X * scaleX);
            int y = imageBounds.Y + (int)Math.Round(point.Y * scaleY);
            return new System.Drawing.Point(x, y);
        }

        private System.Drawing.Rectangle CurrentSelection()
        {
            int left = Math.Clamp(Math.Min(dragStart.X, dragCurrent.X), 0, ClientSize.Width);
            int top = Math.Clamp(Math.Min(dragStart.Y, dragCurrent.Y), 0, ClientSize.Height);
            int right = Math.Clamp(Math.Max(dragStart.X, dragCurrent.X), 0, ClientSize.Width);
            int bottom = Math.Clamp(Math.Max(dragStart.Y, dragCurrent.Y), 0, ClientSize.Height);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private System.Drawing.Rectangle ClientToImage(System.Drawing.Rectangle rect)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            rect.Intersect(imageBounds);
            double scaleX = preview.Width / Math.Max(1.0, (double)imageBounds.Width);
            double scaleY = preview.Height / Math.Max(1.0, (double)imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((rect.X - imageBounds.X) * scaleX), 0, preview.Width - 1);
            int y = Math.Clamp((int)Math.Round((rect.Y - imageBounds.Y) * scaleY), 0, preview.Height - 1);
            int right = Math.Clamp((int)Math.Round((rect.Right - imageBounds.X) * scaleX), x + 1, preview.Width);
            int bottom = Math.Clamp((int)Math.Round((rect.Bottom - imageBounds.Y) * scaleY), y + 1, preview.Height);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Rectangle PreviewImageBounds()
        {
            if (preview.Width <= 0 || preview.Height <= 0 || ClientSize.Width <= 0 || ClientSize.Height <= 0)
            {
                return ClientRectangle;
            }
            double ratio = Math.Min(ClientSize.Width / (double)preview.Width, ClientSize.Height / (double)preview.Height);
            int width = Math.Max(1, (int)Math.Round(preview.Width * ratio));
            int height = Math.Max(1, (int)Math.Round(preview.Height * ratio));
            int x = (ClientSize.Width - width) / 2;
            int y = (ClientSize.Height - height) / 2;
            return new System.Drawing.Rectangle(x, y, width, height);
        }

    }
}
