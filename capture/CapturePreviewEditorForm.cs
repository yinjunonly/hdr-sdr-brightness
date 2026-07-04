using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
    public sealed class PreviewEditorForm : Form
    {
        private enum WindowResizeHit
        {
            None,
            Left,
            Right,
            Top,
            Bottom,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        }

        private readonly Bitmap source;
        private readonly string defaultOutputPath;
        private readonly bool copyOnShown;
        private readonly PictureBox preview = new();
        private readonly PreviewTitleBarControl titleBar = new();
        private readonly PreviewToolbarControl toolbar = new();
        private readonly Panel actionArea = new();
        private readonly Panel content = new();
        private readonly List<EditOperation> operations = new();
        private readonly List<EditOperation> redoOperations = new();
        private Bitmap current;
        private EditMode mode = EditMode.None;
        private AdjustmentPreset adjustmentPreset = AdjustmentPreset.Balanced;
        private int annotationColorIndex;
        private int shapeStrokeWidth = 4;
        private int penStrokeWidth = 6;
        private int mosaicBrushSize = 28;
        private bool dragging;
        private bool movingWindow;
        private bool resizingWindow;
        private WindowResizeHit activeResizeHit = WindowResizeHit.None;
        private Control? chromeCaptureControl;
        private System.Drawing.Point windowDragStartScreen;
        private System.Drawing.Rectangle windowDragStartBounds;
        private System.Drawing.Point dragStart;
        private System.Drawing.Point dragCurrent;
        private readonly List<System.Drawing.Point> currentPenPoints = new();

        public PreviewEditorForm(Bitmap source, string defaultOutputPath, bool copyOnShown = true)
        {
            this.source = CloneAsArgb32(source);
            this.current = new Bitmap(this.source);
            this.defaultOutputPath = defaultOutputPath;
            this.copyOnShown = copyOnShown;
            Text = CaptureText.Get(CaptureString.PreviewTitle);
            StartPosition = FormStartPosition.CenterScreen;
            Size = new System.Drawing.Size(1180, 760);
            MinimumSize = new System.Drawing.Size(900, 520);
            FormBorderStyle = FormBorderStyle.None;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            Padding = new Padding(0);
            BackColor = Color.FromArgb(10, 12, 13);
            ForeColor = Color.White;
            Font = new Font(CaptureText.FontFamily, 9.0f, FontStyle.Regular, GraphicsUnit.Point);

            titleBar.Text = Text;
            titleBar.Font = Font;
            titleBar.CloseClicked += (_, _) => Close();
            AttachWindowChromeMouseHandlers(titleBar);

            preview.Dock = DockStyle.Fill;
            preview.SizeMode = PictureBoxSizeMode.Zoom;
            preview.BackColor = Color.FromArgb(10, 12, 13);
            preview.Image = current;
            preview.MouseDown += PreviewMouseDown;
            preview.MouseMove += PreviewMouseMove;
            preview.MouseUp += PreviewMouseUp;
            preview.Paint += PreviewPaint;
            AttachWindowChromeMouseHandlers(preview);

            toolbar.Font = Font;
            toolbar.Add(ToolbarAction.Cancel, ButtonIcon.Cancel, Close);
            toolbar.Add(ToolbarAction.ToolMarker, ButtonIcon.Marker, anchor => SelectToolbarTool(EditMode.Marker, anchor), () => mode == EditMode.Marker);
            toolbar.Add(ToolbarAction.ToolEllipse, ButtonIcon.Ellipse, anchor => SelectToolbarTool(EditMode.Ellipse, anchor), () => mode == EditMode.Ellipse);
            toolbar.Add(ToolbarAction.ToolPen, ButtonIcon.Pen, anchor => SelectToolbarTool(EditMode.Pen, anchor), () => mode == EditMode.Pen);
            toolbar.Add(ToolbarAction.ToolMosaic, ButtonIcon.Mosaic, anchor => SelectToolbarTool(EditMode.Mosaic, anchor), () => mode == EditMode.Mosaic);
            toolbar.Add(ToolbarAction.Color, ButtonIcon.Color, ShowColorOptions, color: () => AnnotationColors[annotationColorIndex]);
            toolbar.Add(ToolbarAction.Undo, ButtonIcon.Undo, UndoLastEdit);
            toolbar.Add(ToolbarAction.Redo, ButtonIcon.Redo, RedoLastEdit);
            toolbar.Add(ToolbarAction.Reset, ButtonIcon.Reset, ResetEdits);
            toolbar.Add(ToolbarAction.PresetLow, ButtonIcon.Low, () => SetAdjustmentPreset(AdjustmentPreset.Low), () => adjustmentPreset == AdjustmentPreset.Low);
            toolbar.Add(ToolbarAction.PresetBalanced, ButtonIcon.Balanced, () => SetAdjustmentPreset(AdjustmentPreset.Balanced), () => adjustmentPreset == AdjustmentPreset.Balanced);
            toolbar.Add(ToolbarAction.PresetHigh, ButtonIcon.High, () => SetAdjustmentPreset(AdjustmentPreset.High), () => adjustmentPreset == AdjustmentPreset.High);
            toolbar.Add(ToolbarAction.Save, ButtonIcon.Save, SaveToFile);
            toolbar.Add(ToolbarAction.Copy, ButtonIcon.Done, CopyToClipboardAndClose);
            toolbar.OptionsVisibilityChanged += (_, _) => LayoutToolbarOverlay();

            actionArea.Dock = DockStyle.Bottom;
            actionArea.Height = 82;
            actionArea.BackColor = Color.FromArgb(10, 12, 13);
            actionArea.Padding = new Padding(0);
            AttachWindowChromeMouseHandlers(actionArea);

            content.Dock = DockStyle.Fill;
            content.BackColor = Color.FromArgb(10, 12, 13);
            content.Padding = new Padding(0);
            AttachWindowChromeMouseHandlers(content);
            content.Controls.Add(preview);
            content.Controls.Add(actionArea);
            content.Controls.Add(toolbar);
            content.Resize += (_, _) => LayoutToolbarOverlay();
            preview.Resize += (_, _) => LayoutToolbarOverlay();

            Controls.Add(content);
            Controls.Add(titleBar);
            KeyPreview = true;
            LayoutToolbarOverlay();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (copyOnShown)
            {
                BeginInvoke(CopyToClipboard);
            }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            e.Graphics.Clear(BackColor);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Control && e.KeyCode == Keys.Z)
            {
                UndoLastEdit();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.Control && e.KeyCode == Keys.Y)
            {
                RedoLastEdit();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.KeyCode == Keys.Escape) Close();
            base.OnKeyDown(e);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                current.Dispose();
                source.Dispose();
            }
            base.Dispose(disposing);
        }

        private void AttachWindowChromeMouseHandlers(Control control)
        {
            control.MouseDown += WindowChromeMouseDown;
            control.MouseMove += WindowChromeMouseMove;
            control.MouseUp += WindowChromeMouseUp;
            control.MouseLeave += WindowChromeMouseLeave;
        }

        private void WindowChromeMouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left || sender is not Control control) return;
            System.Drawing.Point screenPoint = control.PointToScreen(e.Location);
            System.Drawing.Point clientPoint = PointToClient(screenPoint);
            WindowResizeHit resizeHit = HitTestResize(clientPoint);
            if (resizeHit != WindowResizeHit.None)
            {
                BeginWindowResize(resizeHit, screenPoint, control);
                return;
            }
            if (control == titleBar && !titleBar.IsCloseButtonPoint(e.Location))
            {
                movingWindow = true;
                windowDragStartScreen = screenPoint;
                windowDragStartBounds = Bounds;
                chromeCaptureControl = control;
                control.Capture = true;
            }
        }

        private void WindowChromeMouseMove(object? sender, MouseEventArgs e)
        {
            if (sender is not Control control) return;
            System.Drawing.Point screenPoint = control.PointToScreen(e.Location);
            if (movingWindow)
            {
                int dx = screenPoint.X - windowDragStartScreen.X;
                int dy = screenPoint.Y - windowDragStartScreen.Y;
                Location = new System.Drawing.Point(windowDragStartBounds.Left + dx, windowDragStartBounds.Top + dy);
                return;
            }
            if (resizingWindow)
            {
                ResizeWindow(screenPoint);
                return;
            }

            if (dragging || mode != EditMode.None && control == preview)
            {
                return;
            }
            WindowResizeHit hit = HitTestResize(PointToClient(screenPoint));
            Cursor = CursorForResizeHit(hit);
        }

        private void WindowChromeMouseUp(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;
            movingWindow = false;
            resizingWindow = false;
            activeResizeHit = WindowResizeHit.None;
            if (chromeCaptureControl is not null)
            {
                chromeCaptureControl.Capture = false;
                chromeCaptureControl = null;
            }
            Cursor = Cursors.Default;
        }

        private void WindowChromeMouseLeave(object? sender, EventArgs e)
        {
            if (!movingWindow && !resizingWindow)
            {
                Cursor = Cursors.Default;
            }
        }

        private void BeginWindowResize(WindowResizeHit hit, System.Drawing.Point screenPoint, Control control)
        {
            resizingWindow = true;
            activeResizeHit = hit;
            windowDragStartScreen = screenPoint;
            windowDragStartBounds = Bounds;
            chromeCaptureControl = control;
            control.Capture = true;
        }

        private void ResizeWindow(System.Drawing.Point screenPoint)
        {
            int dx = screenPoint.X - windowDragStartScreen.X;
            int dy = screenPoint.Y - windowDragStartScreen.Y;
            Rectangle next = windowDragStartBounds;
            System.Drawing.Size min = MinimumSize;

            if (activeResizeHit is WindowResizeHit.Left or WindowResizeHit.TopLeft or WindowResizeHit.BottomLeft)
            {
                int newLeft = Math.Min(next.Right - min.Width, windowDragStartBounds.Left + dx);
                next.Width += next.Left - newLeft;
                next.X = newLeft;
            }
            if (activeResizeHit is WindowResizeHit.Right or WindowResizeHit.TopRight or WindowResizeHit.BottomRight)
            {
                next.Width = Math.Max(min.Width, windowDragStartBounds.Width + dx);
            }
            if (activeResizeHit is WindowResizeHit.Top or WindowResizeHit.TopLeft or WindowResizeHit.TopRight)
            {
                int newTop = Math.Min(next.Bottom - min.Height, windowDragStartBounds.Top + dy);
                next.Height += next.Top - newTop;
                next.Y = newTop;
            }
            if (activeResizeHit is WindowResizeHit.Bottom or WindowResizeHit.BottomLeft or WindowResizeHit.BottomRight)
            {
                next.Height = Math.Max(min.Height, windowDragStartBounds.Height + dy);
            }
            Bounds = next;
        }

        private WindowResizeHit HitTestResize(System.Drawing.Point clientPoint)
        {
            int grip = Math.Max(8, (int)Math.Round(10 * DeviceDpi / 96.0));
            bool left = clientPoint.X >= 0 && clientPoint.X < grip;
            bool right = clientPoint.X < ClientSize.Width && clientPoint.X >= ClientSize.Width - grip;
            bool top = clientPoint.Y >= 0 && clientPoint.Y < grip;
            bool bottom = clientPoint.Y < ClientSize.Height && clientPoint.Y >= ClientSize.Height - grip;
            if (top && left) return WindowResizeHit.TopLeft;
            if (top && right) return WindowResizeHit.TopRight;
            if (bottom && left) return WindowResizeHit.BottomLeft;
            if (bottom && right) return WindowResizeHit.BottomRight;
            if (left) return WindowResizeHit.Left;
            if (right) return WindowResizeHit.Right;
            if (top) return WindowResizeHit.Top;
            if (bottom) return WindowResizeHit.Bottom;
            return WindowResizeHit.None;
        }

        private static Cursor CursorForResizeHit(WindowResizeHit hit)
        {
            return hit switch
            {
                WindowResizeHit.Left or WindowResizeHit.Right => Cursors.SizeWE,
                WindowResizeHit.Top or WindowResizeHit.Bottom => Cursors.SizeNS,
                WindowResizeHit.TopLeft or WindowResizeHit.BottomRight => Cursors.SizeNWSE,
                WindowResizeHit.TopRight or WindowResizeHit.BottomLeft => Cursors.SizeNESW,
                _ => Cursors.Default
            };
        }

        private void LayoutToolbarOverlay()
        {
            toolbar.Width = 820;
            toolbar.Height = toolbar.PreferredToolbarHeight;
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            int centerX = content.ClientSize.Width / 2;
            if (imageBounds.Width > 0)
            {
                System.Drawing.Point imageCenter = preview.PointToScreen(new System.Drawing.Point(imageBounds.Left + imageBounds.Width / 2, imageBounds.Bottom));
                centerX = content.PointToClient(imageCenter).X;
            }
            int mainBarTop = content.ClientSize.Height - actionArea.Height + (actionArea.Height - 62) / 2;
            int top = toolbar.OptionsVisible ? mainBarTop - 56 : mainBarTop;
            toolbar.Left = Math.Clamp(centerX - toolbar.Width / 2, 16, Math.Max(16, content.ClientSize.Width - toolbar.Width - 16));
            toolbar.Top = Math.Max(8, top);
            toolbar.BringToFront();
        }

        private void SetStatus(string message)
        {
            _ = message;
        }

        private void UpdateToolButtons()
        {
            toolbar.Invalidate();
        }

        private void SetAdjustmentPreset(AdjustmentPreset preset)
        {
            adjustmentPreset = preset;
            toolbar.Invalidate();
            RebuildCurrent();
        }

        private void SetMode(EditMode nextMode)
        {
            mode = nextMode;
            UpdateToolButtons();
        }

        private void SelectToolbarTool(EditMode nextMode, Rectangle anchor)
        {
            bool alreadyActive = mode == nextMode;
            mode = nextMode;
            toolbar.Invalidate();
            if (alreadyActive)
            {
                ShowToolOptions(nextMode, anchor);
            }
        }

        private void ShowToolOptions(EditMode toolMode, Rectangle anchor)
        {
            int[] values = toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => new[] { 2, 4, 6 },
                EditMode.Pen => new[] { 3, 6, 10 },
                EditMode.Mosaic => new[] { 16, 28, 42 },
                _ => Array.Empty<int>()
            };
            if (values.Length == 0) return;

            List<OptionPopoverItem> options = new();
            foreach (int value in values)
            {
                options.Add(new OptionPopoverItem(
                    Rectangle.Empty,
                    value.ToString(System.Globalization.CultureInfo.InvariantCulture),
                    null,
                    IsToolSizeSelected(toolMode, value),
                    () =>
                    {
                        SetToolSize(toolMode, value);
                        toolbar.Invalidate();
                    }));
            }
            toolbar.ShowOptions(anchor, options);
        }

        private void ShowColorOptions(Rectangle anchor)
        {
            List<OptionPopoverItem> options = new();
            for (int i = 0; i < AnnotationColors.Length; i++)
            {
                int colorIndex = i;
                options.Add(new OptionPopoverItem(
                    Rectangle.Empty,
                    string.Empty,
                    AnnotationColors[colorIndex],
                    annotationColorIndex == colorIndex,
                    () =>
                    {
                        annotationColorIndex = colorIndex;
                        toolbar.Invalidate();
                    }));
            }
            toolbar.ShowOptions(anchor, options);
        }

        private bool IsToolSizeSelected(EditMode toolMode, int value)
        {
            return toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => shapeStrokeWidth == value,
                EditMode.Pen => penStrokeWidth == value,
                EditMode.Mosaic => mosaicBrushSize == value,
                _ => false
            };
        }

        private void SetToolSize(EditMode toolMode, int value)
        {
            if (toolMode is EditMode.Marker or EditMode.Ellipse) shapeStrokeWidth = value;
            if (toolMode == EditMode.Pen) penStrokeWidth = value;
            if (toolMode == EditMode.Mosaic) mosaicBrushSize = value;
        }

        private void CycleAnnotationColor()
        {
            annotationColorIndex = (annotationColorIndex + 1) % AnnotationColors.Length;
            toolbar.Invalidate();
        }

        private void SaveToFile()
        {
            using SaveFileDialog dialog = new()
            {
                Title = CaptureText.Get(CaptureString.SaveDialogTitle),
                Filter = CaptureText.Get(CaptureString.PngFilter),
                FileName = Path.GetFileName(defaultOutputPath),
                InitialDirectory = Path.GetDirectoryName(defaultOutputPath)
            };
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            Directory.CreateDirectory(Path.GetDirectoryName(dialog.FileName)!);
            current.Save(dialog.FileName, ImageFormat.Png);
            SetStatus(CaptureText.Get(CaptureString.SavedStatus));
        }

        private void CopyToClipboard()
        {
            try
            {
                using Bitmap clipboardBitmap = new(current);
                clipboardBitmap.SetResolution(96.0f, 96.0f);
                using MemoryStream pngStream = new();
                clipboardBitmap.Save(pngStream, ImageFormat.Png);
                DataObject data = new();
                data.SetData("PNG", false, new MemoryStream(pngStream.ToArray()));
                data.SetData(DataFormats.Bitmap, true, new Bitmap(clipboardBitmap));
                Clipboard.SetDataObject(data, true, 5, 120);
                SetStatus(CaptureText.Get(CaptureString.CopiedStatus));
            }
            catch (Exception ex)
            {
                SetStatus(CaptureText.Get(CaptureString.CopyFailedPrefix) + ex.Message);
            }
        }

        private void CopyToClipboardAndClose()
        {
            CopyToClipboard();
            Close();
        }

        private void UndoLastEdit()
        {
            if (operations.Count == 0)
            {
                SetStatus(CaptureText.Get(CaptureString.NoUndoStatus));
                return;
            }

            EditOperation operation = operations[^1];
            operations.RemoveAt(operations.Count - 1);
            redoOperations.Add(operation);
            RebuildCurrent();
        }

        private void RedoLastEdit()
        {
            if (redoOperations.Count == 0) return;
            operations.Add(redoOperations[^1]);
            redoOperations.RemoveAt(redoOperations.Count - 1);
            RebuildCurrent();
        }

        private void ResetEdits()
        {
            operations.Clear();
            redoOperations.Clear();
            currentPenPoints.Clear();
            adjustmentPreset = AdjustmentPreset.Balanced;
            annotationColorIndex = 0;
            mode = EditMode.None;
            UpdateToolButtons();
            RebuildCurrent();
        }

        private void RebuildCurrent()
        {
            Bitmap next = BuildAdjustedBitmap();
            foreach (EditOperation operation in operations)
            {
                if (operation.Type == EditOperationType.Mosaic) ApplyMosaic(next, operation.Rect, operation.StrokeWidth);
            }
            using (Graphics graphics = Graphics.FromImage(next))
            {
                graphics.SmoothingMode = SmoothingMode.AntiAlias;
                foreach (EditOperation operation in operations)
                {
                    if (operation.Type == EditOperationType.Mosaic) continue;
                    using Pen pen = new(operation.Color, Math.Max(1, operation.StrokeWidth))
                    {
                        StartCap = LineCap.Round,
                        EndCap = LineCap.Round,
                        LineJoin = LineJoin.Round
                    };
                    if (operation.Type == EditOperationType.Marker) graphics.DrawRectangle(pen, operation.Rect);
                    if (operation.Type == EditOperationType.Ellipse) graphics.DrawEllipse(pen, operation.Rect);
                    if (operation.Type == EditOperationType.Pen && operation.Points is { Count: > 1 })
                    {
                        DrawSegmentedLines(graphics, pen, operation.Points);
                    }
                }
            }

            Bitmap old = current;
            current = next;
            preview.Image = current;
            old.Dispose();
            preview.Invalidate();
        }

        private static Bitmap CloneAsArgb32(Bitmap input)
        {
            Bitmap clone = new(input.Width, input.Height, PixelFormat.Format32bppArgb);
            clone.SetResolution(input.HorizontalResolution, input.VerticalResolution);
            using Graphics graphics = Graphics.FromImage(clone);
            graphics.DrawImageUnscaled(input, 0, 0);
            return clone;
        }

        private unsafe Bitmap BuildAdjustedBitmap()
        {
            (float exposure, float highlightProtect) = adjustmentPreset switch
            {
                AdjustmentPreset.Low => (0.88f, 1.22f),
                AdjustmentPreset.High => (1.12f, 0.82f),
                _ => (1.0f, 1.0f)
            };
            Bitmap output = new(source.Width, source.Height, PixelFormat.Format32bppArgb);
            output.SetResolution(96.0f, 96.0f);

            System.Drawing.Rectangle rect = new(0, 0, source.Width, source.Height);
            BitmapData sourceData = source.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            BitmapData outputData = output.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            try
            {
                for (int y = 0; y < source.Height; y++)
                {
                    byte* sourceRow = (byte*)sourceData.Scan0 + y * sourceData.Stride;
                    byte* outputRow = (byte*)outputData.Scan0 + y * outputData.Stride;
                    for (int x = 0; x < source.Width; x++)
                    {
                        byte* src = sourceRow + x * 4;
                        float b = src[0] / 255.0f * exposure;
                        float g = src[1] / 255.0f * exposure;
                        float r = src[2] / 255.0f * exposure;
                        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        if (luminance > 0.72f)
                        {
                            float over = luminance - 0.72f;
                            float mapped = 0.72f + over / (1.0f + over * 4.0f * highlightProtect);
                            float scale = mapped / Math.Max(0.0001f, luminance);
                            r *= scale;
                            g *= scale;
                            b *= scale;
                        }

                        byte* dst = outputRow + x * 4;
                        dst[0] = PixelMath.ToByte(b);
                        dst[1] = PixelMath.ToByte(g);
                        dst[2] = PixelMath.ToByte(r);
                        dst[3] = 255;
                    }
                }
            }
            finally
            {
                source.UnlockBits(sourceData);
                output.UnlockBits(outputData);
            }

            return output;
        }

        private static void ApplyMosaic(Bitmap bitmap, System.Drawing.Rectangle rect, int preferredBlockSize)
        {
            rect.Intersect(new System.Drawing.Rectangle(0, 0, bitmap.Width, bitmap.Height));
            if (rect.Width <= 0 || rect.Height <= 0) return;
            int block = Math.Max(8, preferredBlockSize);
            using Graphics graphics = Graphics.FromImage(bitmap);
            for (int y = rect.Top; y < rect.Bottom; y += block)
            {
                for (int x = rect.Left; x < rect.Right; x += block)
                {
                    int w = Math.Min(block, rect.Right - x);
                    int h = Math.Min(block, rect.Bottom - y);
                    Color c = bitmap.GetPixel(x + w / 2, y + h / 2);
                    using SolidBrush brush = new(c);
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
        }

        private void PreviewMouseDown(object? sender, MouseEventArgs e)
        {
            if (HitTestResize(PointToClient(preview.PointToScreen(e.Location))) != WindowResizeHit.None) return;
            if (mode == EditMode.None || e.Button != MouseButtons.Left) return;
            dragging = true;
            dragStart = e.Location;
            dragCurrent = e.Location;
            currentPenPoints.Clear();
            if (mode == EditMode.Pen && PreviewImageBounds().Contains(e.Location))
            {
                currentPenPoints.Add(ClientPointToImage(e.Location));
            }
            preview.Capture = true;
            preview.Invalidate();
        }

        private void PreviewMouseMove(object? sender, MouseEventArgs e)
        {
            if (!dragging) return;
            dragCurrent = e.Location;
            if (mode == EditMode.Pen)
            {
                System.Drawing.Rectangle imageBounds = PreviewImageBounds();
                if (imageBounds.Contains(e.Location))
                {
                    currentPenPoints.Add(ClientPointToImage(e.Location));
                }
                else if (currentPenPoints.Count > 0 && !IsPathBreak(currentPenPoints[^1]))
                {
                    currentPenPoints.Add(new System.Drawing.Point(int.MinValue, int.MinValue));
                }
            }
            preview.Invalidate();
        }

        private void PreviewMouseUp(object? sender, MouseEventArgs e)
        {
            if (!dragging || e.Button != MouseButtons.Left) return;
            dragging = false;
            preview.Capture = false;
            dragCurrent = e.Location;
            if (mode == EditMode.Pen)
            {
                TrimPathBreaks(currentPenPoints);
                if (currentPenPoints.Count(point => !IsPathBreak(point)) > 1)
                {
                    operations.Add(new EditOperation(EditOperationType.Pen, System.Drawing.Rectangle.Empty,
                        AnnotationColors[annotationColorIndex], currentPenPoints.ToList(), string.Empty, penStrokeWidth));
                    redoOperations.Clear();
                    RebuildCurrent();
                }
                currentPenPoints.Clear();
                return;
            }

            System.Drawing.Rectangle imageRect = PreviewSelectionToImage();
            if (imageRect.Width < 4 || imageRect.Height < 4) return;
            EditOperationType operationType = mode switch
            {
                EditMode.Ellipse => EditOperationType.Ellipse,
                EditMode.Mosaic => EditOperationType.Mosaic,
                _ => EditOperationType.Marker
            };
            int strokeWidth = mode == EditMode.Mosaic ? mosaicBrushSize : shapeStrokeWidth;
            operations.Add(new EditOperation(operationType, imageRect, AnnotationColors[annotationColorIndex], null, string.Empty, strokeWidth));
            redoOperations.Clear();
            RebuildCurrent();
        }

        private void PreviewPaint(object? sender, PaintEventArgs e)
        {
            if (!dragging) return;
            System.Drawing.Rectangle rect = CurrentPreviewSelection();
            if (rect.Width <= 0 || rect.Height <= 0) return;
            float previewStrokeWidth = mode == EditMode.Pen ? penStrokeWidth : shapeStrokeWidth;
            using Pen pen = new(AnnotationColors[annotationColorIndex], previewStrokeWidth)
            {
                StartCap = LineCap.Round,
                EndCap = LineCap.Round,
                LineJoin = LineJoin.Round
            };
            if (mode == EditMode.Ellipse) e.Graphics.DrawEllipse(pen, rect);
            else if (mode == EditMode.Pen)
            {
                List<System.Drawing.Point> points = currentPenPoints
                    .Select(point => IsPathBreak(point) ? point : ImagePointToPreview(point))
                    .ToList();
                DrawSegmentedLines(e.Graphics, pen, points);
            }
            else e.Graphics.DrawRectangle(pen, rect);
        }

        private System.Drawing.Rectangle CurrentPreviewSelection()
        {
            int left = Math.Min(dragStart.X, dragCurrent.X);
            int top = Math.Min(dragStart.Y, dragCurrent.Y);
            int right = Math.Max(dragStart.X, dragCurrent.X);
            int bottom = Math.Max(dragStart.Y, dragCurrent.Y);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private System.Drawing.Rectangle PreviewSelectionToImage()
        {
            System.Drawing.Rectangle selection = CurrentPreviewSelection();
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            selection.Intersect(imageBounds);
            if (selection.Width <= 0 || selection.Height <= 0) return System.Drawing.Rectangle.Empty;
            double scaleX = current.Width / (double)imageBounds.Width;
            double scaleY = current.Height / (double)imageBounds.Height;
            int x = Math.Clamp((int)Math.Round((selection.X - imageBounds.X) * scaleX), 0, current.Width - 1);
            int y = Math.Clamp((int)Math.Round((selection.Y - imageBounds.Y) * scaleY), 0, current.Height - 1);
            int right = Math.Clamp((int)Math.Round((selection.Right - imageBounds.X) * scaleX), x + 1, current.Width);
            int bottom = Math.Clamp((int)Math.Round((selection.Bottom - imageBounds.Y) * scaleY), y + 1, current.Height);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Point ClientPointToImage(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = current.Width / (double)Math.Max(1, imageBounds.Width);
            double scaleY = current.Height / (double)Math.Max(1, imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((point.X - imageBounds.X) * scaleX), 0, current.Width - 1);
            int y = Math.Clamp((int)Math.Round((point.Y - imageBounds.Y) * scaleY), 0, current.Height - 1);
            return new System.Drawing.Point(x, y);
        }

        private System.Drawing.Point ImagePointToPreview(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / (double)Math.Max(1, current.Width);
            double scaleY = imageBounds.Height / (double)Math.Max(1, current.Height);
            return new System.Drawing.Point(
                imageBounds.X + (int)Math.Round(point.X * scaleX),
                imageBounds.Y + (int)Math.Round(point.Y * scaleY));
        }

        private System.Drawing.Rectangle PreviewImageBounds()
        {
            if (current.Width <= 0 || current.Height <= 0 || preview.Width <= 0 || preview.Height <= 0)
            {
                return preview.ClientRectangle;
            }
            double ratio = Math.Min(preview.Width / (double)current.Width, preview.Height / (double)current.Height);
            int width = (int)Math.Round(current.Width * ratio);
            int height = (int)Math.Round(current.Height * ratio);
            int x = (preview.Width - width) / 2;
            int y = (preview.Height - height) / 2;
            return new System.Drawing.Rectangle(x, y, width, height);
        }
    }
    }
}
