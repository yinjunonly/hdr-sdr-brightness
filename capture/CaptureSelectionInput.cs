using System.Drawing;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
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
            LayoutSelectionToolbar(selected);
            Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            Cursor = Cursors.Cross;
            base.OnMouseLeave(e);
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

    }
}
