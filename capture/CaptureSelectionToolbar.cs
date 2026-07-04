using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
        private readonly ToolbarTooltipLabel selectionTooltip = new();
        private readonly System.Windows.Forms.Timer selectionTooltipTimer = new();

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

    }
}
