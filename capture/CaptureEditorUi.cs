using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;

internal static partial class Program
{
    private const int DwmwaUseImmersiveDarkMode = 20;
    private const int DwmwaUseImmersiveDarkModeLegacy = 19;
    private const int DwmwaBorderColor = 34;
    private const int DwmwaCaptionColor = 35;
    private const int DwmwaTextColor = 36;
    private static readonly nint HwndTopmost = new(-1);
    private static readonly nint HwndNoTopmost = new(-2);
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpShowWindow = 0x0040;
    private const uint SwpHideWindow = 0x0080;
    private const uint SwpNoActivate = 0x0010;
    private const int SwHide = 0;
    private const int WmHotkey = 0x0312;
    private const int VkEscape = 0x1B;

    private sealed partial class RegionSelectionForm : Form
    {
        private const int EscapeHotkeyId = 6201;
        private Bitmap preview;
        private readonly System.Drawing.Rectangle targetBounds;

    public enum EditMode
    {
        None,
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum EditOperationType
    {
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum AdjustmentPreset
    {
        Low,
        Balanced,
        High
    }

        private System.Drawing.Point dragStart;
        private System.Drawing.Point dragCurrent;
        private bool dragging;
        private bool selectionReady;
        private EditMode mode = EditMode.None;
        private AdjustmentPreset selectedPreset = AdjustmentPreset.Balanced;
        private int annotationColorIndex;
        private int shapeStrokeWidth = 4;
        private int penStrokeWidth = 6;
        private int mosaicBrushSize = 28;
        private bool editDragging;
        private System.Drawing.Point editStart;
        private System.Drawing.Point editCurrent;
        private readonly List<System.Drawing.Point> currentPenPoints = new();
        private bool currentStrokeInside;
        private readonly ToolbarTooltipLabel selectionTooltip = new();
        private readonly System.Windows.Forms.Timer selectionTooltipTimer = new();
        private readonly List<EditOperation> operations = new();
        private readonly List<EditOperation> redoOperations = new();
        private Bitmap? selectionBackgroundBitmap;
        private Bitmap? mosaicPreviewBitmap;
        private System.Drawing.Rectangle mosaicPreviewSelection;
        private int mosaicPreviewBrushSize;
        private System.Drawing.Rectangle toolbarBounds;
        private readonly List<ToolbarItem> toolbarItems = new();
        private int hoveredToolbarItem = -1;
        private int pressedToolbarItem = -1;
        private System.Drawing.Rectangle optionsBounds;
        private readonly List<OptionPopoverItem> optionItems = new();
        private bool optionsVisible;
        private int hoveredOptionItem = -1;
        private int pressedOptionItem = -1;
        private bool toolbarTooltipActive;
        private System.Drawing.Point toolbarTooltipAnchor;
        private bool mouseInputReady;
        private bool closingHidden;
        private readonly nint previousForegroundWindow;

        public System.Drawing.Rectangle SelectedImageRegion { get; private set; }

        public SelectionCommitAction CommitAction { get; private set; } = SelectionCommitAction.Copy;

        public AdjustmentPreset SelectedPreset => selectedPreset;

        public IReadOnlyList<EditOperation> Operations => operations;

        public RegionSelectionForm(Bitmap preview)
        {
            this.preview = preview;
            previousForegroundWindow = GetForegroundWindow();
            targetBounds = Screen.PrimaryScreen?.Bounds ?? new System.Drawing.Rectangle(0, 0, preview.Width, preview.Height);
            Bounds = new System.Drawing.Rectangle(-32000, -32000, targetBounds.Width, targetBounds.Height);
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.Manual;
            TopMost = true;
            ShowInTaskbar = false;
            DoubleBuffered = true;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            KeyPreview = true;
            Cursor = Cursors.Cross;
            BackColor = Color.Black;
            mouseInputReady = true;
            Font = new Font(CaptureText.FontFamily, 9.0f, FontStyle.Regular, GraphicsUnit.Point);

            selectionTooltip.Visible = false;
            selectionTooltipTimer.Interval = 1000;
            selectionTooltipTimer.Tick += (_, _) => ShowSelectionTooltip();

            Controls.Add(selectionTooltip);
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            RegisterHotKey(Handle, EscapeHotkeyId, 0, VkEscape);
        }

        protected override void OnHandleDestroyed(EventArgs e)
        {
            UnregisterHotKey(Handle, EscapeHotkeyId);
            base.OnHandleDestroyed(e);
        }

        public void ReplacePreview(Bitmap nextPreview)
        {
            Bitmap oldPreview = preview;
            preview = nextPreview;
            BuildSelectionBackgroundCache();
            DisposeMosaicPreviewCache();
            Invalidate();
            oldPreview.Dispose();
        }

        private void BuildSelectionBackgroundCache()
        {
            selectionBackgroundBitmap?.Dispose();
            selectionBackgroundBitmap = null;
            if (preview.Width <= 0 || preview.Height <= 0) return;

            Bitmap bitmap = new(preview.Width, preview.Height, PixelFormat.Format32bppPArgb);
            bitmap.SetResolution(preview.HorizontalResolution, preview.VerticalResolution);
            using Graphics graphics = Graphics.FromImage(bitmap);
            graphics.Clear(Color.Black);
            graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
            graphics.PixelOffsetMode = PixelOffsetMode.Half;
            graphics.DrawImageUnscaled(preview, 0, 0);
            using SolidBrush shade = new(Color.FromArgb(96, 0, 0, 0));
            graphics.FillRectangle(shade, 0, 0, bitmap.Width, bitmap.Height);
            selectionBackgroundBitmap = bitmap;
        }

        private void InvalidateSelectionChange(System.Drawing.Rectangle oldSelection, System.Drawing.Rectangle newSelection)
        {
            System.Drawing.Rectangle dirty = oldSelection.Width <= 0 || oldSelection.Height <= 0
                ? newSelection
                : newSelection.Width <= 0 || newSelection.Height <= 0
                    ? oldSelection
                    : System.Drawing.Rectangle.Union(oldSelection, newSelection);
            dirty = InflateRectangle(dirty, 18);
            dirty.Intersect(ClientRectangle);
            if (dirty.Width > 0 && dirty.Height > 0)
            {
                Invalidate(dirty);
            }
        }

        private void ShowSelectionTooltip()
        {
            selectionTooltipTimer.Stop();
            if (!toolbarTooltipActive || !selectionReady) return;
            System.Drawing.Size textSize = TextRenderer.MeasureText(selectionTooltip.Text, selectionTooltip.Font);
            selectionTooltip.Width = Math.Max(64, textSize.Width + 24);
            selectionTooltip.Height = 30;
            System.Drawing.Point center = toolbarTooltipAnchor;
            int left = Math.Clamp(center.X - selectionTooltip.Width / 2, 12, Math.Max(12, ClientSize.Width - selectionTooltip.Width - 12));
            int top = toolbarBounds.Top - selectionTooltip.Height - 8;
            if (top < 12) top = toolbarBounds.Bottom + 8;
            selectionTooltip.Location = new System.Drawing.Point(left, top);
            selectionTooltip.Visible = true;
            selectionTooltip.BringToFront();
        }

        private void ShowToolOptions(EditMode toolMode, System.Drawing.Rectangle anchor)
        {
            int[] values = toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => new[] { 2, 4, 6 },
                EditMode.Pen => new[] { 3, 6, 10 },
                EditMode.Mosaic => new[] { 16, 28, 42 },
                _ => Array.Empty<int>()
            };
            if (values.Length == 0)
            {
                HideOptionsPopover();
                return;
            }

            PrepareOptionsPopover(anchor, Math.Max(174, CalculateOptionsWidth(values.Length)));
            for (int index = 0; index < values.Length; index++)
            {
                int value = values[index];
                System.Drawing.Rectangle itemRect = OptionItemRect(index);
                optionItems.Add(new OptionPopoverItem(
                    itemRect,
                    value.ToString(System.Globalization.CultureInfo.InvariantCulture),
                    null,
                    IsToolSizeSelected(toolMode, value),
                    () =>
                    {
                        SetToolSize(toolMode, value);
                        HideOptionsPopover();
                        Invalidate();
                    }));
            }

            ShowOptionsPopover();
        }

        private void ShowColorOptions(System.Drawing.Rectangle anchor)
        {
            PrepareOptionsPopover(anchor, CalculateOptionsWidth(AnnotationColors.Length));
            for (int index = 0; index < AnnotationColors.Length; index++)
            {
                int colorIndex = index;
                System.Drawing.Rectangle itemRect = OptionItemRect(index);
                optionItems.Add(new OptionPopoverItem(
                    itemRect,
                    string.Empty,
                    AnnotationColors[colorIndex],
                    annotationColorIndex == colorIndex,
                    () =>
                    {
                        annotationColorIndex = colorIndex;
                        HideOptionsPopover();
                        Invalidate();
                    }));
            }
            ShowOptionsPopover();
        }

        private static int CalculateOptionsWidth(int count)
        {
            const int itemWidth = 42;
            const int gap = 8;
            const int padX = 12;
            return padX * 2 + count * itemWidth + Math.Max(0, count - 1) * gap;
        }

        private void PrepareOptionsPopover(System.Drawing.Rectangle anchor, int width)
        {
            System.Drawing.Rectangle oldBounds = optionsBounds;
            optionItems.Clear();
            hoveredOptionItem = -1;
            pressedOptionItem = -1;
            optionsBounds = PositionOptionsPanel(anchor, width, 48);
            if (optionsVisible && !oldBounds.IsEmpty) Invalidate(InflateRectangle(oldBounds, 8));
        }

        private System.Drawing.Rectangle OptionItemRect(int index)
        {
            const int itemWidth = 42;
            const int itemHeight = 34;
            const int gap = 8;
            return new System.Drawing.Rectangle(
                optionsBounds.Left + 12 + index * (itemWidth + gap),
                optionsBounds.Top + (optionsBounds.Height - itemHeight) / 2,
                itemWidth,
                itemHeight);
        }

        private void ShowOptionsPopover()
        {
            optionsVisible = optionItems.Count > 0;
            if (optionsVisible)
            {
                selectionTooltip.Visible = false;
                Invalidate(InflateRectangle(optionsBounds, 8));
            }
        }

        private void HideOptionsPopover()
        {
            if (!optionsVisible && optionItems.Count == 0) return;
            System.Drawing.Rectangle oldBounds = optionsBounds;
            optionsVisible = false;
            optionItems.Clear();
            hoveredOptionItem = -1;
            pressedOptionItem = -1;
            if (!oldBounds.IsEmpty) Invalidate(InflateRectangle(oldBounds, 8));
        }

        private int HitTestOptions(System.Drawing.Point point)
        {
            if (!optionsVisible || !optionsBounds.Contains(point)) return -1;
            for (int index = 0; index < optionItems.Count; index++)
            {
                if (optionItems[index].Rect.Contains(point)) return index;
            }
            return -1;
        }

        private void DrawOptionsPopover(Graphics graphics)
        {
            if (!optionsVisible || optionsBounds.Width <= 0 || optionsBounds.Height <= 0) return;
            graphics.SmoothingMode = SmoothingMode.AntiAlias;

            using GraphicsPath shadowPath = RoundedRect(new System.Drawing.Rectangle(optionsBounds.Left + 2, optionsBounds.Top + 4, optionsBounds.Width - 4, optionsBounds.Height - 4), (optionsBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(70, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            System.Drawing.Rectangle pill = new(optionsBounds.Left, optionsBounds.Top, optionsBounds.Width - 1, optionsBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using Region oldClip = graphics.Clip;
            graphics.SetClip(path);
            DrawToolbarBackdrop(graphics, pill);
            graphics.Clip = oldClip;

            using SolidBrush fill = new(Color.FromArgb(184, 27, 30, 33));
            using Pen border = new(Color.FromArgb(96, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            for (int index = 0; index < optionItems.Count; index++)
            {
                OptionPopoverItem item = optionItems[index];
                bool hot = index == hoveredOptionItem || index == pressedOptionItem;
                if (hot || item.Selected)
                {
                    int size = item.Selected ? 34 : 32;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hoverPath = RoundedRect(new System.Drawing.Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 50 : 34, item.Selected ? 64 : 255, item.Selected ? 178 : 255, item.Selected ? 255 : 255));
                    graphics.FillPath(hoverBrush, hoverPath);
                }

                if (item.Color.HasValue)
                {
                    DrawColorOption(graphics, item.Rect, item.Color.Value, item.Selected);
                }
                else
                {
                    Color textColor = item.Selected ? Color.FromArgb(64, 178, 255) : Color.FromArgb(225, 229, 232);
                    TextRenderer.DrawText(graphics, item.Text, Font, item.Rect, textColor,
                        TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                }
            }
        }

        private static void DrawColorOption(Graphics graphics, System.Drawing.Rectangle rect, Color color, bool selected)
        {
            int diameter = selected ? 21 : 19;
            int x = rect.Left + (rect.Width - diameter) / 2;
            int y = rect.Top + (rect.Height - diameter) / 2;
            using SolidBrush swatch = new(color);
            using Pen border = new(color.ToArgb() == Color.White.ToArgb()
                ? Color.FromArgb(220, 225, 229, 232)
                : Color.FromArgb(90, 255, 255, 255), 1.2f);
            graphics.FillEllipse(swatch, x, y, diameter, diameter);
            graphics.DrawEllipse(border, x, y, diameter, diameter);
            if (selected)
            {
                using Pen ring = new(Color.FromArgb(220, 225, 229, 232), 1.5f);
                graphics.DrawEllipse(ring, x - 4, y - 4, diameter + 8, diameter + 8);
            }
        }

        private System.Drawing.Rectangle PositionOptionsPanel(System.Drawing.Rectangle anchor, int width, int height)
        {
            System.Drawing.Point center = new(anchor.Left + anchor.Width / 2, anchor.Top);
            int left = Math.Clamp(center.X - width / 2, 12, Math.Max(12, ClientSize.Width - width - 12));
            int top = toolbarBounds.Top - height - 8;
            if (top < 12) top = toolbarBounds.Bottom + 8;
            return new System.Drawing.Rectangle(left, top, width, height);
        }

        private bool IsToolSizeSelected(EditMode toolMode, int value) => toolMode switch
        {
            EditMode.Marker or EditMode.Ellipse => shapeStrokeWidth == value,
            EditMode.Pen => penStrokeWidth == value,
            EditMode.Mosaic => mosaicBrushSize == value,
            _ => false
        };

        private void SetToolSize(EditMode toolMode, int value)
        {
            if (toolMode is EditMode.Marker or EditMode.Ellipse) shapeStrokeWidth = value;
            if (toolMode == EditMode.Pen) penStrokeWidth = value;
            if (toolMode == EditMode.Mosaic)
            {
                mosaicBrushSize = value;
                DisposeMosaicPreviewCache();
            }
        }

        private void UndoSelectionOperation()
        {
            if (operations.Count == 0) return;
            redoOperations.Add(operations[^1]);
            operations.RemoveAt(operations.Count - 1);
            Invalidate();
        }

        private void RedoSelectionOperation()
        {
            if (redoOperations.Count == 0) return;
            operations.Add(redoOperations[^1]);
            redoOperations.RemoveAt(redoOperations.Count - 1);
            Invalidate();
        }

        private void ResetSelectionOperations()
        {
            operations.Clear();
            redoOperations.Clear();
            Invalidate();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            BuildSelectionBackgroundCache();
            Refresh();
            Bounds = targetBounds;
            SetWindowPos(Handle, HwndTopmost, targetBounds.Left, targetBounds.Top,
                targetBounds.Width, targetBounds.Height, SwpShowWindow);
            BuildSelectionBackgroundCache();
            Refresh();
            BringToFront();
            SetForegroundWindow(Handle);
            Activate();
            Focus();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                selectionTooltipTimer.Dispose();
                selectionBackgroundBitmap?.Dispose();
                mosaicPreviewBitmap?.Dispose();
                preview.Dispose();
            }
            base.Dispose(disposing);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            HideBeforeClose();
            base.OnFormClosing(e);
        }

        private void HideBeforeClose()
        {
            if (closingHidden || !IsHandleCreated) return;
            closingHidden = true;
            try
            {
                SetWindowPos(Handle, HwndNoTopmost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpNoActivate);
                if (previousForegroundWindow != 0 &&
                    previousForegroundWindow != Handle &&
                    IsWindow(previousForegroundWindow))
                {
                    SetForegroundWindow(previousForegroundWindow);
                }
                ShowWindow(Handle, SwHide);
                SetWindowPos(Handle, HwndTopmost, -32000, -32000,
                    Math.Max(1, Width), Math.Max(1, Height), SwpHideWindow | SwpNoActivate);
                Hide();
            }
            catch
            {
            }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            e.Graphics.Clear(Color.Black);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
            e.Graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            if (selectionBackgroundBitmap is null ||
                selectionBackgroundBitmap.Width != preview.Width ||
                selectionBackgroundBitmap.Height != preview.Height)
            {
                BuildSelectionBackgroundCache();
            }
            if (selectionBackgroundBitmap is not null)
            {
                e.Graphics.DrawImage(selectionBackgroundBitmap, imageBounds);
            }
            else
            {
                e.Graphics.DrawImage(preview, imageBounds);
                using SolidBrush shade = new(Color.FromArgb(96, 0, 0, 0));
                e.Graphics.FillRectangle(shade, ClientRectangle);
            }

            if (!selectionReady && !dragging)
            {
                string hint = CaptureText.Get(CaptureString.SelectHint);
                using Font hintFont = new(CaptureText.FontFamily, 13.0f, FontStyle.Regular, GraphicsUnit.Point);
                SizeF hintSize = e.Graphics.MeasureString(hint, hintFont);
                System.Drawing.RectangleF hintBox = new(24, 24, hintSize.Width + 28, hintSize.Height + 16);
                using SolidBrush hintBack = new(Color.FromArgb(190, 24, 24, 24));
                using SolidBrush hintText = new(Color.White);
                e.Graphics.FillRectangle(hintBack, hintBox);
                e.Graphics.DrawString(hint, hintFont, hintText, hintBox.X + 14, hintBox.Y + 8);
            }

            System.Drawing.Rectangle selection = CurrentSelection();
            if (selection.Width > 0 && selection.Height > 0)
            {
                using Region oldClip = e.Graphics.Clip;
                e.Graphics.SetClip(selection);
                e.Graphics.DrawImage(preview, imageBounds);
                DrawPresetPreview(e.Graphics, selection);
                e.Graphics.Clip = oldClip;
                using Pen border = new(Color.FromArgb(0, 120, 215), 2.5f);
                e.Graphics.DrawRectangle(border, selection);
                using Pen inner = new(Color.White, 1.0f);
                e.Graphics.DrawRectangle(inner, System.Drawing.Rectangle.Inflate(selection, -2, -2));
                DrawSelectionHandles(e.Graphics, selection);
                DrawSelectionOperations(e.Graphics);
                if (editDragging)
                {
                    System.Drawing.Rectangle editRect = CurrentEditSelection();
                    editRect.Intersect(selection);
                    if (mode == EditMode.Mosaic)
                    {
                        using Region oldEditClip = e.Graphics.Clip;
                        e.Graphics.SetClip(InsetRectangle(selection, Math.Max(2, mosaicBrushSize / 2 + 2)));
                        DrawMosaicPreview(e.Graphics, ClientToImage(editRect), editRect, false);
                        e.Graphics.Clip = oldEditClip;
                    }
                    else if (mode == EditMode.Pen)
                    {
                        using Region oldEditClip = e.Graphics.Clip;
                        e.Graphics.SetClip(selection);
                        DrawClientPolyline(e.Graphics, currentPenPoints, AnnotationColors[annotationColorIndex], penStrokeWidth);
                        e.Graphics.Clip = oldEditClip;
                    }
                    else if (mode == EditMode.Ellipse)
                    {
                        using Pen editPen = new(AnnotationColors[annotationColorIndex], shapeStrokeWidth);
                        e.Graphics.DrawEllipse(editPen, editRect);
                    }
                    else
                    {
                        using Pen editPen = new(AnnotationColors[annotationColorIndex], shapeStrokeWidth);
                        e.Graphics.DrawRectangle(editPen, editRect);
                    }
                }
            }
            if (selectionReady)
            {
                DrawSelectionToolbar(e.Graphics);
                DrawOptionsPopover(e.Graphics);
            }
        }

        private void DrawSelectionToolbar(Graphics graphics)
        {
            if (toolbarBounds.Width <= 0 || toolbarBounds.Height <= 0) return;
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using GraphicsPath shadowPath = RoundedRect(new System.Drawing.Rectangle(toolbarBounds.Left + 2, toolbarBounds.Top + 4, toolbarBounds.Width - 4, toolbarBounds.Height - 4), (toolbarBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            System.Drawing.Rectangle pill = new(toolbarBounds.Left, toolbarBounds.Top, toolbarBounds.Width - 1, toolbarBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using Region oldClip = graphics.Clip;
            graphics.SetClip(path);
            DrawToolbarBackdrop(graphics, pill);
            graphics.Clip = oldClip;

            using SolidBrush fill = new(Color.FromArgb(178, 27, 30, 33));
            using Pen border = new(Color.FromArgb(95, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            foreach (ToolbarItem item in toolbarItems)
            {
                if (item.Action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
                {
                    DrawToolbarSeparator(graphics, item.Rect.Left - 11);
                }

                bool selected = IsToolbarItemSelected(item.Action);
                bool hot = toolbarItems.IndexOf(item) == hoveredToolbarItem || toolbarItems.IndexOf(item) == pressedToolbarItem;
                Color iconColor = ToolbarIconColor(item.Action, selected);
                if (hot || selected)
                {
                    int size = hot ? 38 : 34;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hover = RoundedRect(new System.Drawing.Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 44 : 28, selected ? 64 : 255, selected ? 178 : 255, selected ? 255 : 255));
                    graphics.FillPath(hoverBrush, hover);
                }

                System.Drawing.Rectangle iconRect = new(item.Rect.Left + 10, item.Rect.Top + 8, 26, 26);
                if (item.Action == ToolbarAction.Color)
                {
                    iconColor = AnnotationColors[annotationColorIndex];
                }
                PillButton.DrawIcon(graphics, iconRect, item.Icon, iconColor);
            }
        }

        private void DrawToolbarBackdrop(Graphics graphics, System.Drawing.Rectangle pill)
        {
            try
            {
                System.Drawing.Rectangle imageRect = ClientToImage(pill);
                if (imageRect.Width <= 0 || imageRect.Height <= 0) return;
                int smallW = Math.Max(1, pill.Width / 10);
                int smallH = Math.Max(1, pill.Height / 10);
                using Bitmap small = new(smallW, smallH, PixelFormat.Format32bppArgb);
                using (Graphics smallGraphics = Graphics.FromImage(small))
                {
                    smallGraphics.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    smallGraphics.DrawImage(preview, new System.Drawing.Rectangle(0, 0, smallW, smallH), imageRect, GraphicsUnit.Pixel);
                }
                using Bitmap blurred = new(pill.Width, pill.Height, PixelFormat.Format32bppArgb);
                using (Graphics blurGraphics = Graphics.FromImage(blurred))
                {
                    blurGraphics.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    blurGraphics.DrawImage(small, new System.Drawing.Rectangle(0, 0, blurred.Width, blurred.Height));
                    using SolidBrush dim = new(Color.FromArgb(135, 0, 0, 0));
                    blurGraphics.FillRectangle(dim, 0, 0, blurred.Width, blurred.Height);
                }
                graphics.DrawImageUnscaled(blurred, pill.Location);
            }
            catch
            {
            }
        }

        private void DrawToolbarSeparator(Graphics graphics, int x)
        {
            using Pen pen = new(Color.FromArgb(70, 255, 255, 255), 1.0f);
            graphics.DrawLine(pen, x, toolbarBounds.Top + 19, x, toolbarBounds.Bottom - 19);
        }

        private bool IsToolbarItemSelected(ToolbarAction action)
        {
            return action switch
            {
                ToolbarAction.ToolMarker => mode == EditMode.Marker,
                ToolbarAction.ToolEllipse => mode == EditMode.Ellipse,
                ToolbarAction.ToolPen => mode == EditMode.Pen,
                ToolbarAction.ToolMosaic => mode == EditMode.Mosaic,
                ToolbarAction.PresetLow => selectedPreset == AdjustmentPreset.Low,
                ToolbarAction.PresetBalanced => selectedPreset == AdjustmentPreset.Balanced,
                ToolbarAction.PresetHigh => selectedPreset == AdjustmentPreset.High,
                _ => false
            };
        }

        private static Color ToolbarIconColor(ToolbarAction action, bool selected)
        {
            if (action == ToolbarAction.Cancel) return Color.FromArgb(255, 92, 92);
            if (action == ToolbarAction.Copy) return Color.FromArgb(68, 214, 111);
            if (selected) return Color.FromArgb(64, 178, 255);
            return Color.FromArgb(225, 229, 232);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            if (e.Button != MouseButtons.Left) return;
            if (optionsVisible)
            {
                pressedOptionItem = HitTestOptions(e.Location);
                if (pressedOptionItem >= 0)
                {
                    Capture = true;
                    Invalidate(InflateRectangle(optionsBounds, 8));
                    return;
                }

                if (!toolbarBounds.Contains(e.Location))
                {
                    HideOptionsPopover();
                }
            }
            if (selectionReady && toolbarBounds.Contains(e.Location))
            {
                pressedToolbarItem = HitTestToolbar(e.Location);
                if (pressedToolbarItem >= 0)
                {
                    Capture = true;
                    Invalidate(toolbarBounds);
                    return;
                }
            }
            if (selectionReady && mode != EditMode.None && CurrentSelection().Contains(e.Location))
            {
                editDragging = true;
                editStart = e.Location;
                editCurrent = e.Location;
                currentPenPoints.Clear();
                currentStrokeInside = CurrentSelection().Contains(e.Location);
                if (mode == EditMode.Pen && currentStrokeInside) currentPenPoints.Add(e.Location);
                Capture = true;
                Invalidate();
                return;
            }
            selectionReady = false;
            HideToolbarFeedback();
            operations.Clear();
            redoOperations.Clear();
            DisposeMosaicPreviewCache();
            dragging = true;
            dragStart = e.Location;
            dragCurrent = e.Location;
            Capture = true;
            Invalidate();
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            UpdateSelectionCursor(e.Location);
            if (optionsVisible)
            {
                int optionHover = HitTestOptions(e.Location);
                if (optionHover != hoveredOptionItem)
                {
                    hoveredOptionItem = optionHover;
                    Invalidate(InflateRectangle(optionsBounds, 8));
                }
            }
            if (!dragging && !editDragging && selectionReady)
            {
                int hover = HitTestToolbar(e.Location);
                if (hover != hoveredToolbarItem)
                {
                    hoveredToolbarItem = hover;
                    UpdateToolbarTooltip(hover);
                    Invalidate(toolbarBounds);
                }
            }
            if (editDragging)
            {
                editCurrent = e.Location;
                if (mode == EditMode.Pen)
                {
                    bool inside = CurrentSelection().Contains(e.Location);
                    if (inside)
                    {
                        if (!currentStrokeInside && currentPenPoints.Count > 0)
                        {
                            currentPenPoints.Add(new System.Drawing.Point(int.MinValue, int.MinValue));
                        }
                        if (ShouldAppendStrokePoint(e.Location))
                        {
                            currentPenPoints.Add(e.Location);
                        }
                    }
                    currentStrokeInside = inside;
                }
                Invalidate();
                return;
            }
            if (!dragging) return;
            System.Drawing.Rectangle oldSelection = CurrentSelection();
            dragCurrent = e.Location;
            InvalidateSelectionChange(oldSelection, CurrentSelection());
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            if (pressedOptionItem >= 0 && e.Button == MouseButtons.Left)
            {
                int pressed = pressedOptionItem;
                pressedOptionItem = -1;
                Capture = false;
                if (pressed == HitTestOptions(e.Location) && pressed >= 0 && pressed < optionItems.Count)
                {
                    optionItems[pressed].Click();
                }
                else
                {
                    Invalidate(InflateRectangle(optionsBounds, 8));
                }
                return;
            }
            if (pressedToolbarItem >= 0 && e.Button == MouseButtons.Left)
            {
                int pressed = pressedToolbarItem;
                pressedToolbarItem = -1;
                Capture = false;
                if (pressed == HitTestToolbar(e.Location))
                {
                    ExecuteToolbarAction(toolbarItems[pressed]);
                }
                Invalidate();
                return;
            }
            if (editDragging && e.Button == MouseButtons.Left)
            {
                editDragging = false;
                Capture = false;
                editCurrent = e.Location;
                System.Drawing.Rectangle activeSelection = CurrentSelection();
                if (mode == EditMode.Pen)
                {
                    List<System.Drawing.Point> points = currentPenPoints
                        .Where(point => IsPathBreak(point) || activeSelection.Contains(point))
                        .Select(point => IsPathBreak(point) ? point : ClientPointToImage(point))
                        .ToList();
                    if (points.Count(point => !IsPathBreak(point)) > 1)
                    {
                        operations.Add(new EditOperation(
                            EditOperationType.Pen,
                            System.Drawing.Rectangle.Empty,
                            AnnotationColors[annotationColorIndex],
                            points,
                            string.Empty,
                            penStrokeWidth));
                        redoOperations.Clear();
                    }
                    currentPenPoints.Clear();
                    currentStrokeInside = false;
                }
                else
                {
                    System.Drawing.Rectangle editRect = CurrentEditSelection();
                    editRect.Intersect(activeSelection);
                    if (editRect.Width >= 4 && editRect.Height >= 4)
                    {
                        EditOperationType operationType = mode switch
                        {
                            EditMode.Ellipse => EditOperationType.Ellipse,
                            EditMode.Mosaic => EditOperationType.Mosaic,
                            _ => EditOperationType.Marker
                        };
                        int strokeWidth = mode == EditMode.Mosaic ? mosaicBrushSize : shapeStrokeWidth;
                        operations.Add(new EditOperation(operationType, ClientToImage(editRect), AnnotationColors[annotationColorIndex], null, string.Empty, strokeWidth));
                        redoOperations.Clear();
                    }
                }
                Invalidate();
                return;
            }
            if (!dragging || e.Button != MouseButtons.Left) return;
            dragging = false;
            Capture = false;
            dragCurrent = e.Location;
            System.Drawing.Rectangle selected = CurrentSelection();
            if (selected.Width < 4 || selected.Height < 4)
            {
                selectionReady = false;
                HideToolbarFeedback();
                Invalidate();
                return;
            }
            SelectedImageRegion = ClientToImage(selected);
            selectionReady = true;
            DisposeMosaicPreviewCache();
            LayoutSelectionToolbar(selected);
            Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            Cursor = Cursors.Cross;
            base.OnMouseLeave(e);
        }

        private void CommitSelection(SelectionCommitAction action)
        {
            if (!selectionReady || SelectedImageRegion.Width <= 0 || SelectedImageRegion.Height <= 0) return;
            CommitAction = action;
            HideBeforeClose();
            DialogResult = DialogResult.OK;
            Close();
        }

        private void LayoutSelectionToolbar(System.Drawing.Rectangle selection)
        {
            int width = 820;
            int height = 62;
            int left = selection.Left + (selection.Width - width) / 2;
            left = Math.Clamp(left, 12, Math.Max(12, ClientSize.Width - width - 12));
            int top = selection.Bottom + 10;
            if (top + height > ClientSize.Height - 12)
            {
                top = selection.Top - height - 10;
            }
            top = Math.Clamp(top, 12, Math.Max(12, ClientSize.Height - height - 12));
            toolbarBounds = new System.Drawing.Rectangle(left, top, width, height);
            LayoutToolbarItems();
        }

        private void LayoutToolbarItems()
        {
            toolbarItems.Clear();
            if (toolbarBounds.Width <= 0 || toolbarBounds.Height <= 0) return;
            int x = toolbarBounds.Left + 14;
            int y = toolbarBounds.Top + (toolbarBounds.Height - 42) / 2;
            void Add(ToolbarAction action, ButtonIcon icon, string tooltip)
            {
                toolbarItems.Add(new ToolbarItem(action, icon, new System.Drawing.Rectangle(x, y, 46, 42), tooltip));
                x += 54;
            }
            void Sep() => x += 13;

            Add(ToolbarAction.Cancel, ButtonIcon.Cancel, CaptureText.Get(CaptureString.ToolbarCancel));
            Add(ToolbarAction.ToolMarker, ButtonIcon.Marker, CaptureText.Get(CaptureString.ToolbarMarker));
            Add(ToolbarAction.ToolEllipse, ButtonIcon.Ellipse, CaptureText.Get(CaptureString.ToolbarEllipse));
            Add(ToolbarAction.ToolPen, ButtonIcon.Pen, CaptureText.Get(CaptureString.ToolbarPen));
            Add(ToolbarAction.ToolMosaic, ButtonIcon.Mosaic, CaptureText.Get(CaptureString.ToolbarMosaic));
            Add(ToolbarAction.Color, ButtonIcon.Color, CaptureText.Get(CaptureString.ToolbarColor));
            Sep();
            Add(ToolbarAction.Undo, ButtonIcon.Undo, CaptureText.Get(CaptureString.ToolbarUndo));
            Add(ToolbarAction.Redo, ButtonIcon.Redo, CaptureText.Get(CaptureString.ToolbarRedo));
            Add(ToolbarAction.Reset, ButtonIcon.Reset, CaptureText.Get(CaptureString.ToolbarReset));
            Sep();
            Add(ToolbarAction.PresetLow, ButtonIcon.Low, CaptureText.Get(CaptureString.ToolbarHdrLow));
            Add(ToolbarAction.PresetBalanced, ButtonIcon.Balanced, CaptureText.Get(CaptureString.ToolbarHdrBalanced));
            Add(ToolbarAction.PresetHigh, ButtonIcon.High, CaptureText.Get(CaptureString.ToolbarHdrHigh));
            Sep();
            Add(ToolbarAction.Save, ButtonIcon.Save, CaptureText.Get(CaptureString.ToolbarSave));
            Add(ToolbarAction.Copy, ButtonIcon.Done, CaptureText.Get(CaptureString.ToolbarCopy));
        }

        private int HitTestToolbar(System.Drawing.Point point)
        {
            if (!toolbarBounds.Contains(point)) return -1;
            for (int i = 0; i < toolbarItems.Count; i++)
            {
                if (toolbarItems[i].Rect.Contains(point)) return i;
            }
            return -1;
        }

        private void UpdateSelectionCursor(System.Drawing.Point point)
        {
            if (dragging || editDragging)
            {
                if (Cursor != Cursors.Cross) Cursor = Cursors.Cross;
                return;
            }

            Cursor desired = Cursors.Cross;
            if (selectionReady)
            {
                if (HitTestOptions(point) >= 0 || HitTestToolbar(point) >= 0)
                {
                    desired = Cursors.Hand;
                }
                else if ((optionsVisible && optionsBounds.Contains(point)) || toolbarBounds.Contains(point))
                {
                    desired = Cursors.Default;
                }
            }

            if (Cursor != desired) Cursor = desired;
        }

        private void HideToolbarFeedback()
        {
            hoveredToolbarItem = -1;
            pressedToolbarItem = -1;
            toolbarTooltipActive = false;
            selectionTooltipTimer.Stop();
            selectionTooltip.Visible = false;
            HideOptionsPopover();
        }

        private void UpdateToolbarTooltip(int hover)
        {
            toolbarTooltipActive = hover >= 0;
            selectionTooltip.Visible = false;
            selectionTooltipTimer.Stop();
            if (hover < 0)
            {
                return;
            }
            ToolbarItem item = toolbarItems[hover];
            selectionTooltip.Text = item.Tooltip;
            toolbarTooltipAnchor = new System.Drawing.Point(item.Rect.Left + item.Rect.Width / 2, item.Rect.Top);
            selectionTooltipTimer.Start();
        }

        private void ExecuteToolbarAction(ToolbarItem item)
        {
            switch (item.Action)
            {
                case ToolbarAction.Cancel:
                    CancelSelection();
                    break;
                case ToolbarAction.ToolMarker:
                    SelectToolbarTool(EditMode.Marker, item.Rect);
                    break;
                case ToolbarAction.ToolEllipse:
                    SelectToolbarTool(EditMode.Ellipse, item.Rect);
                    break;
                case ToolbarAction.ToolPen:
                    SelectToolbarTool(EditMode.Pen, item.Rect);
                    break;
                case ToolbarAction.ToolMosaic:
                    SelectToolbarTool(EditMode.Mosaic, item.Rect);
                    break;
                case ToolbarAction.Color:
                    ShowColorOptions(item.Rect);
                    break;
                case ToolbarAction.Undo:
                    UndoSelectionOperation();
                    break;
                case ToolbarAction.Redo:
                    RedoSelectionOperation();
                    break;
                case ToolbarAction.Reset:
                    ResetSelectionOperations();
                    break;
                case ToolbarAction.PresetLow:
                    SetToolbarPreset(AdjustmentPreset.Low);
                    break;
                case ToolbarAction.PresetBalanced:
                    SetToolbarPreset(AdjustmentPreset.Balanced);
                    break;
                case ToolbarAction.PresetHigh:
                    SetToolbarPreset(AdjustmentPreset.High);
                    break;
                case ToolbarAction.Save:
                    CommitSelection(SelectionCommitAction.Save);
                    break;
                case ToolbarAction.Copy:
                    CommitSelection(SelectionCommitAction.Copy);
                    break;
            }
        }

        private void SelectToolbarTool(EditMode nextMode, System.Drawing.Rectangle anchor)
        {
            bool alreadyActive = mode == nextMode;
            mode = nextMode;
            HideOptionsPopover();
            if (alreadyActive)
            {
                ShowToolOptions(nextMode, anchor);
            }
            Invalidate();
        }

        private void SetToolbarPreset(AdjustmentPreset preset)
        {
            selectedPreset = preset;
            Invalidate();
        }

        private void DrawSelectionOperations(Graphics graphics)
        {
            System.Drawing.Rectangle selection = CurrentSelection();
            foreach (EditOperation operation in operations)
            {
                System.Drawing.Rectangle rect = ImageToClient(operation.Rect);
                if (operation.Type == EditOperationType.Marker)
                {
                    using Pen pen = new(operation.Color, operation.StrokeWidth);
                    graphics.DrawRectangle(pen, rect);
                }
                else if (operation.Type == EditOperationType.Ellipse)
                {
                    using Pen pen = new(operation.Color, operation.StrokeWidth);
                    graphics.DrawEllipse(pen, rect);
                }
                else if (operation.Type == EditOperationType.Pen && operation.Points is not null)
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(selection);
                    DrawClientPolyline(graphics, operation.Points.Select(point => IsPathBreak(point) ? point : ImagePointToClient(point)).ToList(), operation.Color, operation.StrokeWidth);
                    graphics.Clip = oldClip;
                }
                else if (operation.Type == EditOperationType.Mosaic && operation.Points is not null)
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(InsetRectangle(selection, Math.Max(2, operation.StrokeWidth / 2 + 2)));
                    DrawMosaicBrushPreview(graphics, operation.Points.Select(point => IsPathBreak(point) ? point : ImagePointToClient(point)).ToList(), operation.StrokeWidth);
                    graphics.Clip = oldClip;
                }
                else
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(InsetRectangle(selection, Math.Max(2, operation.StrokeWidth / 2 + 2)));
                    DrawMosaicPreview(graphics, operation.Rect, rect, false);
                    graphics.Clip = oldClip;
                }
            }
        }

        private void DrawPresetPreview(Graphics graphics, System.Drawing.Rectangle selection)
        {
            if (selectedPreset == AdjustmentPreset.Balanced) return;
            Color overlay = selectedPreset == AdjustmentPreset.High
                ? Color.FromArgb(34, 255, 244, 210)
                : Color.FromArgb(50, 0, 0, 0);
            using SolidBrush brush = new(overlay);
            graphics.FillRectangle(brush, selection);
        }

        private static void DrawClientPolyline(Graphics graphics, IReadOnlyList<System.Drawing.Point> points, Color color, int width)
        {
            if (points.Count < 2) return;
            using Pen pen = new(color, Math.Max(1, width)) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            List<System.Drawing.Point> segment = new();
            foreach (System.Drawing.Point point in points)
            {
                if (IsPathBreak(point))
                {
                    if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
                    segment.Clear();
                    continue;
                }
                segment.Add(point);
            }
            if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
        }

        private void DrawMosaicBrushPreview(Graphics graphics, IReadOnlyList<System.Drawing.Point> points, int brushSize)
        {
            if (points.Count == 0) return;
            int radius = Math.Max(4, brushSize / 2);
            using Region oldClip = graphics.Clip;
            using GraphicsPath path = BuildBrushPath(points, radius);
            System.Drawing.Rectangle selection = CurrentSelection();
            System.Drawing.Rectangle bounds = System.Drawing.Rectangle.Round(path.GetBounds());
            bounds.Intersect(InsetRectangle(selection, Math.Max(2, brushSize / 2 + 2)));
            if (bounds.Width <= 0 || bounds.Height <= 0)
            {
                graphics.Clip = oldClip;
                return;
            }

            graphics.SetClip(path);
            int block = Math.Max(10, brushSize);
            for (int y = bounds.Top; y < bounds.Bottom; y += block)
            {
                for (int x = bounds.Left; x < bounds.Right; x += block)
                {
                    int w = Math.Min(block, bounds.Right - x);
                    int h = Math.Min(block, bounds.Bottom - y);
                    using SolidBrush brush = new(PseudoPreviewBlockColor(selection, x, y, w, h));
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
            graphics.Clip = oldClip;
        }

        private bool ShouldAppendStrokePoint(System.Drawing.Point point)
        {
            int minDistance = mode == EditMode.Mosaic
                ? Math.Max(3, mosaicBrushSize / 6)
                : Math.Max(2, penStrokeWidth);
            if (currentPenPoints.Count == 0) return true;
            System.Drawing.Point previous = currentPenPoints[^1];
            if (IsPathBreak(previous)) return true;
            int dx = point.X - previous.X;
            int dy = point.Y - previous.Y;
            return (dx * dx + dy * dy) >= minDistance * minDistance;
        }

        private Color PseudoPreviewBlockColor(System.Drawing.Rectangle selection, int x, int y, int blockWidth, int blockHeight)
        {
            double u = selection.Width <= 1 ? 0.0 : (x + blockWidth * 0.5 - selection.Left) / Math.Max(1.0, selection.Width - 1.0);
            double v = selection.Height <= 1 ? 0.0 : (y + blockHeight * 0.5 - selection.Top) / Math.Max(1.0, selection.Height - 1.0);
            u = Math.Clamp(u, 0.0, 1.0);
            v = Math.Clamp(v, 0.0, 1.0);
            Color top = PreviewColorAt(selection.Left + (int)Math.Round(u * Math.Max(0, selection.Width - 1)), selection.Top);
            Color bottom = PreviewColorAt(selection.Left + (int)Math.Round(u * Math.Max(0, selection.Width - 1)), selection.Bottom - 1);
            Color left = PreviewColorAt(selection.Left, selection.Top + (int)Math.Round(v * Math.Max(0, selection.Height - 1)));
            Color right = PreviewColorAt(selection.Right - 1, selection.Top + (int)Math.Round(v * Math.Max(0, selection.Height - 1)));
            double horizontalWeight = Math.Clamp(0.5 + (Math.Min(u, 1.0 - u) - Math.Min(v, 1.0 - v)), 0.15, 0.85);
            double verticalWeight = 1.0 - horizontalWeight;
            int noise = DeterministicNoise(x / Math.Max(1, blockWidth), y / Math.Max(1, blockHeight));
            byte r = QuantizeColor((long)(horizontalWeight * Lerp(left.R, right.R, u) + verticalWeight * Lerp(top.R, bottom.R, v) + noise));
            byte g = QuantizeColor((long)(horizontalWeight * Lerp(left.G, right.G, u) + verticalWeight * Lerp(top.G, bottom.G, v) + noise));
            byte b = QuantizeColor((long)(horizontalWeight * Lerp(left.B, right.B, u) + verticalWeight * Lerp(top.B, bottom.B, v) + noise));
            return Color.FromArgb(r, g, b);
        }

        private Color PreviewColorAt(int clientX, int clientY)
        {
            System.Drawing.Point imagePoint = ClientPointToImage(new System.Drawing.Point(clientX, clientY));
            return preview.GetPixel(imagePoint.X, imagePoint.Y);
        }

        private Color AveragePreviewBlock(int x, int y, int blockWidth, int blockHeight)
        {
            long rSum = 0;
            long gSum = 0;
            long bSum = 0;
            int samples = 0;
            int step = Math.Max(1, Math.Min(blockWidth, blockHeight) / 4);
            for (int py = y; py < y + blockHeight; py += step)
            {
                for (int px = x; px < x + blockWidth; px += step)
                {
                    System.Drawing.Point imagePoint = ClientPointToImage(new System.Drawing.Point(px, py));
                    Color color = preview.GetPixel(imagePoint.X, imagePoint.Y);
                    rSum += color.R;
                    gSum += color.G;
                    bSum += color.B;
                    samples++;
                }
            }
            samples = Math.Max(1, samples);
            return Color.FromArgb(QuantizeColor(rSum / samples), QuantizeColor(gSum / samples), QuantizeColor(bSum / samples));
        }

        private Bitmap? GetMosaicPreviewBitmap(System.Drawing.Rectangle selection, int brushSize)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            selection.Intersect(imageBounds);
            if (selection.Width <= 0 || selection.Height <= 0) return null;
            System.Drawing.Rectangle imageSelection = ClientToImage(selection);
            if (mosaicPreviewBitmap is not null &&
                mosaicPreviewSelection == imageSelection &&
                mosaicPreviewBrushSize == brushSize)
            {
                return mosaicPreviewBitmap;
            }

            DisposeMosaicPreviewCache();
            using Bitmap crop = preview.Clone(imageSelection, PixelFormat.Format32bppArgb);
            int pixelSize = Math.Max(18, brushSize * 2);
            using Bitmap pixelatedCrop = CreatePseudoPixelatedBitmap(crop, pixelSize);
            Bitmap cached = new(selection.Width, selection.Height, PixelFormat.Format32bppArgb);
            cached.SetResolution(96.0f, 96.0f);
            using (Graphics cachedGraphics = Graphics.FromImage(cached))
            {
                cachedGraphics.InterpolationMode = InterpolationMode.NearestNeighbor;
                cachedGraphics.PixelOffsetMode = PixelOffsetMode.Half;
                cachedGraphics.DrawImage(pixelatedCrop, new Rectangle(0, 0, cached.Width, cached.Height));
            }
            mosaicPreviewSelection = imageSelection;
            mosaicPreviewBrushSize = brushSize;
            mosaicPreviewBitmap = cached;
            return mosaicPreviewBitmap;
        }

        private void DisposeMosaicPreviewCache()
        {
            mosaicPreviewBitmap?.Dispose();
            mosaicPreviewBitmap = null;
            mosaicPreviewSelection = System.Drawing.Rectangle.Empty;
            mosaicPreviewBrushSize = 0;
        }

        private void DrawMosaicPreview(Graphics graphics, System.Drawing.Rectangle imageRect, System.Drawing.Rectangle clientRect, bool drawBorder = false)
        {
            if (imageRect.Width <= 0 || imageRect.Height <= 0 || clientRect.Width <= 0 || clientRect.Height <= 0) return;
            int imageBlock = Math.Max(10, Math.Max(12, mosaicBrushSize));
            int clientBlock = Math.Max(6, (int)Math.Round(imageBlock * (clientRect.Width / Math.Max(1.0, (double)imageRect.Width))));
            using Region oldClip = graphics.Clip;
            graphics.SetClip(clientRect);
            for (int y = clientRect.Top; y < clientRect.Bottom; y += clientBlock)
            {
                for (int x = clientRect.Left; x < clientRect.Right; x += clientBlock)
                {
                    int w = Math.Min(clientBlock, clientRect.Right - x);
                    int h = Math.Min(clientBlock, clientRect.Bottom - y);
                    using SolidBrush brush = new(PseudoPreviewBlockColor(clientRect, x, y, w, h));
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
            graphics.Clip = oldClip;
            if (drawBorder)
            {
                using Pen pen = new(Color.White, 1.5f);
                graphics.DrawRectangle(pen, clientRect);
            }
        }

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

        private static void DrawSelectionHandles(Graphics graphics, System.Drawing.Rectangle selection)
        {
            using SolidBrush brush = new(Color.White);
            int size = 8;
            int half = size / 2;
            System.Drawing.Point[] points =
            {
                new(selection.Left, selection.Top),
                new(selection.Left + selection.Width / 2, selection.Top),
                new(selection.Right, selection.Top),
                new(selection.Right, selection.Top + selection.Height / 2),
                new(selection.Right, selection.Bottom),
                new(selection.Left + selection.Width / 2, selection.Bottom),
                new(selection.Left, selection.Bottom),
                new(selection.Left, selection.Top + selection.Height / 2)
            };
            foreach (System.Drawing.Point point in points)
            {
                graphics.FillRectangle(brush, point.X - half, point.Y - half, size, size);
            }
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (!mouseInputReady) return;
            if (e.Control && e.KeyCode == Keys.Z)
            {
                UndoSelectionOperation();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.Control && e.KeyCode == Keys.Y)
            {
                RedoSelectionOperation();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.KeyCode == Keys.Escape)
            {
                CancelSelection();
            }
            base.OnKeyDown(e);
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WmHotkey && m.WParam.ToInt32() == EscapeHotkeyId)
            {
                CancelSelection();
                return;
            }

            base.WndProc(ref m);
        }

        private void CancelSelection()
        {
            HideBeforeClose();
            DialogResult = DialogResult.Cancel;
            Close();
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

    private static void ApplyDarkWindowFrame(nint hwnd)
    {
        if (hwnd == 0) return;

        int enabled = 1;
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, ref enabled, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkModeLegacy, ref enabled, sizeof(int));

        int captionColor = ColorRef(Color.FromArgb(15, 17, 18));
        int borderColor = ColorRef(Color.FromArgb(55, 60, 64));
        int textColor = ColorRef(Color.FromArgb(235, 239, 242));
        _ = DwmSetWindowAttribute(hwnd, DwmwaCaptionColor, ref captionColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaBorderColor, ref borderColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaTextColor, ref textColor, sizeof(int));
    }

    private static int ColorRef(Color color)
    {
        return color.R | (color.G << 8) | (color.B << 16);
    }

    [DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern bool IsWindow(nint hwnd);

    [DllImport("user32.dll")]
    private static extern bool SetWindowDisplayAffinity(nint hwnd, uint affinity);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(nint hwnd, int id, uint fsModifiers, int vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(nint hwnd, int id);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(nint hwnd, nint hwndInsertAfter, int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(nint hwnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(nint hwnd);

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(nint hwnd, int attribute, ref int attributeValue, int attributeSize);

}
