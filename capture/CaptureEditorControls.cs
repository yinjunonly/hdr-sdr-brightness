using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
    private enum ButtonIcon
    {
        None,
        Marker,
        Ellipse,
        Pen,
        Color,
        Mosaic,
        Undo,
        Redo,
        Reset,
        Save,
        Cancel,
        Done,
        Low,
        Balanced,
        High
    }

    private enum ToolbarAction
    {
        Cancel,
        ToolMarker,
        ToolEllipse,
        ToolPen,
        ToolMosaic,
        Color,
        Undo,
        Redo,
        Reset,
        PresetLow,
        PresetBalanced,
        PresetHigh,
        Save,
        Copy
    }

    private readonly record struct ToolbarItem(
        ToolbarAction Action,
        ButtonIcon Icon,
        System.Drawing.Rectangle Rect,
        string Tooltip);

    private static readonly Color[] AnnotationColors =
    {
        Color.FromArgb(255, 59, 48),
        Color.FromArgb(255, 204, 0),
        Color.FromArgb(52, 199, 89),
        Color.FromArgb(64, 156, 255),
        Color.FromArgb(191, 90, 242),
        Color.White
    };

    public enum SelectionCommitAction
    {
        Copy,
        Save
    }

    public sealed record EditOperation(
        EditOperationType Type,
        System.Drawing.Rectangle Rect,
        Color Color,
        IReadOnlyList<System.Drawing.Point>? Points = null,
        string Text = "",
        int StrokeWidth = 2);

    public readonly record struct RegionSelectionResult(
        System.Drawing.Rectangle Region,
        SelectionCommitAction Action,
        AdjustmentPreset Preset,
        IReadOnlyList<EditOperation> Operations);

    private sealed class PillButton : Button
    {
        private static readonly Color ToolbarBackColor = Color.FromArgb(28, 31, 34);
        private bool hot;
        private bool pressed;

        public bool Primary { get; set; }

        public bool Selected { get; set; }

        public ButtonIcon Icon { get; set; }

        public PillButton()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            FlatStyle = FlatStyle.Flat;
            FlatAppearance.BorderSize = 0;
            UseVisualStyleBackColor = false;
            BackColor = ToolbarBackColor;
            ForeColor = Color.White;
            Cursor = Cursors.Hand;
        }

        protected override void OnMouseEnter(EventArgs e)
        {
            hot = true;
            Invalidate();
            base.OnMouseEnter(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            hot = false;
            pressed = false;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                pressed = true;
                Invalidate();
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            pressed = false;
            Invalidate();
            base.OnMouseUp(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? ToolbarBackColor);

            Rectangle rect = new(1, 1, Width - 3, Height - 3);
            Rectangle iconRect = Icon != ButtonIcon.None
                ? string.IsNullOrEmpty(Text)
                    ? new Rectangle((Width - 26) / 2, (Height - 26) / 2, 26, 26)
                    : new Rectangle(rect.Left + 12, (Height - 18) / 2, 18, 18)
                : rect;
            if (hot || pressed)
            {
                int size = Math.Min(38, Math.Min(rect.Width, rect.Height));
                int centerX = iconRect.Left + iconRect.Width / 2;
                int centerY = iconRect.Top + iconRect.Height / 2;
                Rectangle hoverRect = new(centerX - size / 2, centerY - size / 2, size, size);
                using GraphicsPath hoverPath = RoundedRect(hoverRect, size / 2);
                using SolidBrush hoverBrush = new(Color.FromArgb(pressed ? 58 : 36, 255, 255, 255));
                e.Graphics.FillPath(hoverBrush, hoverPath);
            }

            Color iconColor = Icon switch
            {
                ButtonIcon.Cancel => Color.FromArgb(255, 92, 92),
                ButtonIcon.Done => Color.FromArgb(68, 214, 111),
                ButtonIcon.Color => ForeColor,
                _ when Primary || Selected => Color.FromArgb(64, 178, 255),
                _ => Color.FromArgb(225, 229, 232)
            };

            if (Icon != ButtonIcon.None)
            {
                DrawIcon(e.Graphics, iconRect, Icon, iconColor);
            }

            if (!string.IsNullOrEmpty(Text))
            {
                Rectangle textRect = Icon == ButtonIcon.None ? rect : new Rectangle(rect.Left + 34, rect.Top, rect.Width - 40, rect.Height);
                TextRenderer.DrawText(e.Graphics, Text, Font, textRect, iconColor,
                    TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
            }
        }

        public static void DrawIcon(Graphics graphics, Rectangle rect, ButtonIcon icon, Color color)
        {
            string glyph = icon switch
            {
                ButtonIcon.Marker => string.Empty,
                ButtonIcon.Ellipse => "\uEA3A",
                ButtonIcon.Pen => "\uED63",
                ButtonIcon.Color => "\uE790",
                ButtonIcon.Mosaic => string.Empty,
                ButtonIcon.Undo => "\uE7A7",
                ButtonIcon.Redo => "\uE7A6",
                ButtonIcon.Reset => "\uE72C",
                ButtonIcon.Save => "\uE74E",
                ButtonIcon.Cancel => string.Empty,
                ButtonIcon.Done => string.Empty,
                ButtonIcon.Low => string.Empty,
                ButtonIcon.Balanced => string.Empty,
                ButtonIcon.High => string.Empty,
                _ => string.Empty
            };
            if (!string.IsNullOrEmpty(glyph))
            {
                DrawFluentGlyph(graphics, rect, glyph, color);
                if (icon is ButtonIcon.Low or ButtonIcon.Balanced or ButtonIcon.High)
                {
                    int level = icon == ButtonIcon.Low ? 1 : icon == ButtonIcon.Balanced ? 2 : 3;
                    using SolidBrush levelBrush = new(color);
                    using Pen levelPen = new(color, 1.4f);
                    for (int i = 0; i < 3; i++)
                    {
                        Rectangle dot = new(rect.Left + 5 + i * 7, rect.Bottom - 2, 3, 3);
                        if (i < level) graphics.FillEllipse(levelBrush, dot);
                        else graphics.DrawEllipse(levelPen, dot);
                    }
                }
                return;
            }

            using Pen pen = new(color, 2.1f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            using SolidBrush brush = new(color);
            int cx = rect.Left + rect.Width / 2;
            int cy = rect.Top + rect.Height / 2;
            switch (icon)
            {
                case ButtonIcon.Marker:
                    graphics.DrawLine(pen, rect.Left + 4, rect.Top + 6, rect.Right - 4, rect.Top + 6);
                    graphics.DrawLine(pen, rect.Right - 4, rect.Top + 6, rect.Right - 4, rect.Bottom - 6);
                    graphics.DrawLine(pen, rect.Right - 4, rect.Bottom - 6, rect.Left + 4, rect.Bottom - 6);
                    graphics.DrawLine(pen, rect.Left + 4, rect.Bottom - 6, rect.Left + 4, rect.Top + 6);
                    break;
                case ButtonIcon.Ellipse:
                    graphics.DrawEllipse(pen, rect.Left + 4, rect.Top + 5, rect.Width - 8, rect.Height - 10);
                    break;
                case ButtonIcon.Pen:
                    graphics.DrawLine(pen, rect.Left + 6, rect.Bottom - 5, rect.Right - 5, rect.Top + 6);
                    graphics.DrawLine(pen, rect.Right - 8, rect.Top + 5, rect.Right - 4, rect.Top + 9);
                    graphics.DrawLine(pen, rect.Left + 5, rect.Bottom - 5, rect.Left + 9, rect.Bottom - 4);
                    break;
                case ButtonIcon.Color:
                    using (Pen ring = new(Color.FromArgb(230, 225, 229, 232), 1.5f))
                    {
                        graphics.DrawEllipse(ring, rect.Left + 4, rect.Top + 4, rect.Width - 8, rect.Height - 8);
                    }
                    graphics.FillEllipse(brush, rect.Left + 7, rect.Top + 7, rect.Width - 14, rect.Height - 14);
                    using (SolidBrush shine = new(Color.FromArgb(210, 255, 255, 255)))
                    {
                        graphics.FillEllipse(shine, rect.Left + 10, rect.Top + 9, 4, 4);
                    }
                    break;
                case ButtonIcon.Mosaic:
                    using (SolidBrush dimBrush = new(Color.FromArgb(115, color)))
                    {
                        int cell = 5;
                        int gap = 2;
                        int left = cx - (cell * 3 + gap * 2) / 2;
                        int top = cy - (cell * 3 + gap * 2) / 2;
                        for (int y = 0; y < 3; y++)
                        {
                            for (int x = 0; x < 3; x++)
                            {
                                Rectangle cellRect = new(left + x * (cell + gap), top + y * (cell + gap), cell, cell);
                                if (((x + y) & 1) == 0) graphics.FillRectangle(brush, cellRect);
                                else graphics.FillRectangle(dimBrush, cellRect);
                            }
                        }
                    }
                    break;
                case ButtonIcon.Undo:
                    graphics.DrawArc(pen, rect.Left + 4, rect.Top + 4, 13, 13, 200, 230);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Left + 5, rect.Top + 10), new System.Drawing.Point(rect.Left + 1, rect.Top + 10), new System.Drawing.Point(rect.Left + 4, rect.Top + 6) });
                    break;
                case ButtonIcon.Reset:
                    graphics.DrawArc(pen, rect.Left + 4, rect.Top + 4, 13, 13, 30, 300);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Right - 4, rect.Top + 7), new System.Drawing.Point(rect.Right - 2, rect.Top + 3), new System.Drawing.Point(rect.Right - 7, rect.Top + 4) });
                    break;
                case ButtonIcon.Save:
                    graphics.DrawRectangle(pen, rect.Left + 4, rect.Top + 3, rect.Width - 8, rect.Height - 6);
                    graphics.DrawLine(pen, cx, rect.Top + 6, cx, rect.Bottom - 6);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(cx - 4, rect.Bottom - 10), new System.Drawing.Point(cx, rect.Bottom - 6), new System.Drawing.Point(cx + 4, rect.Bottom - 10) });
                    break;
                case ButtonIcon.Cancel:
                    graphics.DrawLine(pen, rect.Left + 5, rect.Top + 5, rect.Right - 5, rect.Bottom - 5);
                    graphics.DrawLine(pen, rect.Right - 5, rect.Top + 5, rect.Left + 5, rect.Bottom - 5);
                    break;
                case ButtonIcon.Done:
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Left + 3, cy), new System.Drawing.Point(cx - 2, rect.Bottom - 5), new System.Drawing.Point(rect.Right - 3, rect.Top + 5) });
                    break;
                case ButtonIcon.Low:
                case ButtonIcon.Balanced:
                case ButtonIcon.High:
                    int level = icon == ButtonIcon.Low ? 1 : icon == ButtonIcon.Balanced ? 2 : 3;
                    int core = 8;
                    graphics.FillEllipse(brush, cx - core / 2, cy - core / 2, core, core);
                    int rays = level == 1 ? 4 : level == 2 ? 6 : 8;
                    int inner = 9;
                    int outer = level == 1 ? 11 : level == 2 ? 12 : 13;
                    for (int ray = 0; ray < rays; ray++)
                    {
                        double angle = Math.PI * 2 * ray / rays - Math.PI / 2;
                        int x1 = cx + (int)Math.Round(Math.Cos(angle) * inner);
                        int y1 = cy + (int)Math.Round(Math.Sin(angle) * inner);
                        int x2 = cx + (int)Math.Round(Math.Cos(angle) * outer);
                        int y2 = cy + (int)Math.Round(Math.Sin(angle) * outer);
                        graphics.DrawLine(pen, x1, y1, x2, y2);
                    }
                    break;
            }
        }

        private static void DrawFluentGlyph(Graphics graphics, Rectangle rect, string glyph, Color color)
        {
            using Font font = CreateIconFont(22.0f);
            TextRenderer.DrawText(graphics, glyph, font, rect, color,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }

        private static Font CreateIconFont(float size)
        {
            try
            {
                return new Font("Segoe Fluent Icons", size, FontStyle.Regular, GraphicsUnit.Pixel);
            }
            catch
            {
                return new Font("Segoe MDL2 Assets", size, FontStyle.Regular, GraphicsUnit.Pixel);
            }
        }
    }

    private sealed class ModernStatusLabel : Control
    {
        public ModernStatusLabel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            ForeColor = Color.FromArgb(231, 235, 238);
        }

        protected override void OnTextChanged(EventArgs e)
        {
            Invalidate();
            base.OnTextChanged(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.FromArgb(15, 17, 18));

            Rectangle rect = new(0, 2, Width - 1, Height - 4);
            using GraphicsPath path = RoundedRect(rect, 10);
            using SolidBrush fill = new(Color.FromArgb(31, 36, 39));
            using Pen border = new(Color.FromArgb(52, 59, 63), 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);

            using SolidBrush dot = new(Color.FromArgb(0, 103, 192));
            e.Graphics.FillEllipse(dot, 14, (Height - 7) / 2, 7, 7);
            Rectangle textRect = new(30, 0, Math.Max(0, Width - 42), Height);
            TextRenderer.DrawText(e.Graphics, Text, Font, textRect, ForeColor,
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
        }
    }

    private sealed class ToolbarTooltipLabel : Control
    {
        public ToolbarTooltipLabel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            BackColor = Color.FromArgb(31, 33, 36);
            ForeColor = Color.White;
            Font = new Font("Microsoft YaHei UI", 9.0f, FontStyle.Regular, GraphicsUnit.Point);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            Rectangle rect = new(0, 0, Width - 1, Height - 1);
            using GraphicsPath path = RoundedRect(rect, 8);
            using SolidBrush fill = new(Color.FromArgb(242, 32, 34, 37));
            e.Graphics.FillPath(fill, path);
            TextRenderer.DrawText(e.Graphics, Text, Font, rect, ForeColor,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
        }
    }

    private sealed class FloatingToolbarPanel : Panel
    {
        private static readonly Color ToolbarBackColor = Color.FromArgb(28, 31, 34);
        private static readonly Color ToolbarBorderColor = Color.FromArgb(95, 255, 255, 255);

        public FloatingToolbarPanel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.SupportsTransparentBackColor, true);
            BackColor = Color.FromArgb(28, 31, 34);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.Black);

            Rectangle shadowRect = new(2, 4, Width - 5, Height - 7);
            using GraphicsPath shadowPath = RoundedRect(shadowRect, Math.Max(1, shadowRect.Height / 2));
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            e.Graphics.FillPath(shadow, shadowPath);

            Rectangle rect = new(0, 0, Width - 2, Height - 2);
            using GraphicsPath path = RoundedRect(rect, Math.Max(1, rect.Height / 2));
            using SolidBrush fill = new(Color.FromArgb(178, ToolbarBackColor));
            using Pen border = new(ToolbarBorderColor, 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);
        }
    }

    private sealed class PreviewToolbarControl : Control
    {
        private readonly List<PreviewToolbarItem> items = new();
        private readonly List<OptionPopoverItem> optionItems = new();
        private int hoveredIndex = -1;
        private int pressedIndex = -1;
        private int hoveredOptionIndex = -1;
        private int pressedOptionIndex = -1;
        private Rectangle optionsBounds;
        private bool optionsVisible;

        public event EventHandler? OptionsVisibilityChanged;

        public PreviewToolbarControl()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.SupportsTransparentBackColor, true);
            BackColor = Color.Transparent;
            Cursor = Cursors.Default;
            Height = 62;
            Width = 820;
        }

        public bool OptionsVisible => optionsVisible;

        public int PreferredToolbarHeight => optionsVisible ? 118 : 62;

        public void Add(ToolbarAction action, ButtonIcon icon, Action<Rectangle> click, Func<bool>? selected = null, Func<Color>? color = null)
        {
            int x = items.Count == 0 ? 14 : items[^1].Rect.Right + 8;
            if (action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
            {
                x += 13;
            }
            Rectangle rect = new(x, 10, 46, 42);
            items.Add(new PreviewToolbarItem(action, icon, rect, click, selected, color));
        }

        public void Add(ToolbarAction action, ButtonIcon icon, Action click, Func<bool>? selected = null, Func<Color>? color = null)
        {
            Add(action, icon, _ => click(), selected, color);
        }

        public void ShowOptions(Rectangle anchor, IReadOnlyList<OptionPopoverItem> options)
        {
            optionItems.Clear();
            int width = 24 + options.Count * 42 + Math.Max(0, options.Count - 1) * 8;
            int left = Math.Clamp(anchor.Left + (anchor.Width - width) / 2, 0, Math.Max(0, Width - width));
            optionsBounds = new Rectangle(left, 0, width, 48);
            for (int i = 0; i < options.Count; i++)
            {
                OptionPopoverItem item = options[i];
                Rectangle rect = new(optionsBounds.Left + 12 + i * 50, optionsBounds.Top + 7, 42, 34);
                optionItems.Add(new OptionPopoverItem(rect, item.Text, item.Color, item.Selected, item.Click));
            }
            optionsVisible = optionItems.Count > 0;
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Height = PreferredToolbarHeight;
            OptionsVisibilityChanged?.Invoke(this, EventArgs.Empty);
            Invalidate();
        }

        public void HideOptions()
        {
            if (!optionsVisible && optionItems.Count == 0) return;
            optionsVisible = false;
            optionItems.Clear();
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Height = PreferredToolbarHeight;
            OptionsVisibilityChanged?.Invoke(this, EventArgs.Empty);
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.Black);

            if (optionsVisible)
            {
                DrawOptions(e.Graphics);
            }

            int barTop = BarTop();
            Rectangle shadowRect = new(2, barTop + 4, Width - 5, 55);
            using GraphicsPath shadowPath = RoundedRect(shadowRect, Math.Max(1, shadowRect.Height / 2));
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            e.Graphics.FillPath(shadow, shadowPath);

            Rectangle pill = new(0, barTop, Width - 2, 60);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using SolidBrush fill = new(Color.FromArgb(178, 27, 30, 33));
            using Pen border = new(Color.FromArgb(95, 255, 255, 255), 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);

            foreach (PreviewToolbarItem item in items)
            {
                Rectangle itemRect = OffsetBarRect(item.Rect);
                if (item.Action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
                {
                    DrawSeparator(e.Graphics, itemRect.Left - 11);
                }

                int index = items.IndexOf(item);
                bool selected = item.Selected?.Invoke() == true;
                bool hot = index == hoveredIndex || index == pressedIndex;
                Color iconColor = item.Color?.Invoke() ?? ToolbarIconColor(item.Action, selected);
                if (hot || selected)
                {
                    int size = hot ? 38 : 34;
                    int cx = itemRect.Left + itemRect.Width / 2;
                    int cy = itemRect.Top + itemRect.Height / 2;
                    using GraphicsPath hover = RoundedRect(new Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 44 : 28, selected ? 64 : 255, selected ? 178 : 255, selected ? 255 : 255));
                    e.Graphics.FillPath(hoverBrush, hover);
                }

                Rectangle iconRect = new(itemRect.Left + 10, itemRect.Top + 8, 26, 26);
                PillButton.DrawIcon(e.Graphics, iconRect, item.Icon, iconColor);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            int optionHit = HitTestOptions(e.Location);
            int hit = optionHit >= 0 ? -1 : HitTest(e.Location);
            if (optionHit != hoveredOptionIndex || hit != hoveredIndex)
            {
                hoveredOptionIndex = optionHit;
                hoveredIndex = hit;
                Cursor = hit >= 0 || optionHit >= 0 ? Cursors.Hand : Cursors.Default;
                Invalidate();
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            hoveredIndex = -1;
            pressedIndex = -1;
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Cursor = Cursors.Default;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                pressedOptionIndex = HitTestOptions(e.Location);
                pressedIndex = pressedOptionIndex >= 0 ? -1 : HitTest(e.Location);
                if (pressedIndex < 0 && pressedOptionIndex < 0)
                {
                    HideOptions();
                }
                Invalidate();
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            int pressed = pressedIndex;
            int pressedOption = pressedOptionIndex;
            pressedIndex = -1;
            pressedOptionIndex = -1;
            if (e.Button == MouseButtons.Left && pressedOption >= 0 && pressedOption == HitTestOptions(e.Location))
            {
                Action click = optionItems[pressedOption].Click;
                HideOptions();
                click();
                Invalidate();
                base.OnMouseUp(e);
                return;
            }
            if (e.Button == MouseButtons.Left && pressed >= 0 && pressed == HitTest(e.Location))
            {
                items[pressed].Click(items[pressed].Rect);
            }
            Invalidate();
            base.OnMouseUp(e);
        }

        private int HitTest(System.Drawing.Point point)
        {
            for (int i = 0; i < items.Count; i++)
            {
                if (OffsetBarRect(items[i].Rect).Contains(point)) return i;
            }
            return -1;
        }

        private int HitTestOptions(System.Drawing.Point point)
        {
            if (!optionsVisible || !optionsBounds.Contains(point)) return -1;
            for (int i = 0; i < optionItems.Count; i++)
            {
                if (optionItems[i].Rect.Contains(point)) return i;
            }
            return -1;
        }

        private int BarTop() => optionsVisible ? 56 : 0;

        private Rectangle OffsetBarRect(Rectangle rect)
        {
            return new Rectangle(rect.Left, rect.Top + BarTop(), rect.Width, rect.Height);
        }

        private void DrawSeparator(Graphics graphics, int x)
        {
            using Pen pen = new(Color.FromArgb(70, 255, 255, 255), 1.0f);
            int barTop = BarTop();
            graphics.DrawLine(pen, x, barTop + 19, x, barTop + 43);
        }

        private void DrawOptions(Graphics graphics)
        {
            using GraphicsPath shadowPath = RoundedRect(new Rectangle(optionsBounds.Left + 2, optionsBounds.Top + 4, optionsBounds.Width - 4, optionsBounds.Height - 4), (optionsBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(70, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            Rectangle pill = new(optionsBounds.Left, optionsBounds.Top, optionsBounds.Width - 1, optionsBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using SolidBrush fill = new(Color.FromArgb(184, 27, 30, 33));
            using Pen border = new(Color.FromArgb(96, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            for (int i = 0; i < optionItems.Count; i++)
            {
                OptionPopoverItem item = optionItems[i];
                bool hot = i == hoveredOptionIndex || i == pressedOptionIndex;
                if (hot || item.Selected)
                {
                    int size = item.Selected ? 34 : 32;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hoverPath = RoundedRect(new Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
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

        private static Color ToolbarIconColor(ToolbarAction action, bool selected)
        {
            if (action == ToolbarAction.Cancel) return Color.FromArgb(255, 92, 92);
            if (action == ToolbarAction.Copy) return Color.FromArgb(68, 214, 111);
            if (selected) return Color.FromArgb(64, 178, 255);
            return Color.FromArgb(225, 229, 232);
        }
    }

    private sealed record PreviewToolbarItem(
        ToolbarAction Action,
        ButtonIcon Icon,
        Rectangle Rect,
        Action<Rectangle> Click,
        Func<bool>? Selected,
        Func<Color>? Color);

    private sealed class OptionPopoverItem
    {
        public OptionPopoverItem(System.Drawing.Rectangle rect, string text, Color? color, bool selected, Action click)
        {
            Rect = rect;
            Text = text;
            Color = color;
            Selected = selected;
            Click = click;
        }

        public System.Drawing.Rectangle Rect { get; }

        public string Text { get; }

        public Color? Color { get; }

        public bool Selected { get; }

        public Action Click { get; }
    }

    private sealed class PreviewTitleBarControl : Control
    {
        private bool closeHot;
        private bool closePressed;

        public event EventHandler? CloseClicked;

        public PreviewTitleBarControl()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            BackColor = Color.FromArgb(10, 12, 13);
            ForeColor = Color.FromArgb(235, 239, 242);
            Height = 52;
            Dock = DockStyle.Top;
        }

        public Rectangle CloseButtonBounds => new(Width - 58, 8, 42, 36);

        public bool IsCloseButtonPoint(System.Drawing.Point point) => CloseButtonBounds.Contains(point);

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(BackColor);
            using SolidBrush titleBrush = new(ForeColor);
            TextRenderer.DrawText(e.Graphics, Text, Font, new Rectangle(14, 0, Math.Max(1, Width - 86), Height),
                ForeColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);

            Rectangle closeRect = CloseButtonBounds;
            Color iconColor = closeHot || closePressed ? Color.White : Color.FromArgb(224, 229, 232);
            if (closeHot || closePressed)
            {
                Color closeColor = closePressed
                    ? Color.FromArgb(198, 35, 48)
                    : Color.FromArgb(228, 43, 58);
                using GraphicsPath closePath = RoundedRect(closeRect, 9);
                using SolidBrush closeBrush = new(closeColor);
                e.Graphics.FillPath(closeBrush, closePath);
            }

            using Pen pen = new(iconColor, 2.0f) { StartCap = LineCap.Round, EndCap = LineCap.Round };
            int cx = closeRect.Left + closeRect.Width / 2;
            int cy = closeRect.Top + closeRect.Height / 2;
            e.Graphics.DrawLine(pen, cx - 6, cy - 6, cx + 6, cy + 6);
            e.Graphics.DrawLine(pen, cx + 6, cy - 6, cx - 6, cy + 6);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            bool hot = CloseButtonBounds.Contains(e.Location);
            if (hot != closeHot)
            {
                closeHot = hot;
                Cursor = closeHot ? Cursors.Hand : Cursors.Default;
                Invalidate(CloseButtonBounds);
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            closeHot = false;
            closePressed = false;
            Cursor = Cursors.Default;
            Invalidate(CloseButtonBounds);
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left && CloseButtonBounds.Contains(e.Location))
            {
                closePressed = true;
                Invalidate(CloseButtonBounds);
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            bool shouldClose = closePressed && e.Button == MouseButtons.Left && CloseButtonBounds.Contains(e.Location);
            closePressed = false;
            Invalidate(CloseButtonBounds);
            if (shouldClose)
            {
                CloseClicked?.Invoke(this, EventArgs.Empty);
            }
            base.OnMouseUp(e);
        }
    }

    private static GraphicsPath RoundedRect(Rectangle rect, int radius)
    {
        GraphicsPath path = new();
        int arc = Math.Max(1, radius * 2);
        path.AddArc(rect.Left, rect.Top, arc, arc, 180, 90);
        path.AddArc(rect.Right - arc, rect.Top, arc, arc, 270, 90);
        path.AddArc(rect.Right - arc, rect.Bottom - arc, arc, arc, 0, 90);
        path.AddArc(rect.Left, rect.Bottom - arc, arc, arc, 90, 90);
        path.CloseFigure();
        return path;
    }

    }
}
