#pragma once

#include <windows.h>

#include <cstddef>
#include <vector>

namespace editor {

struct BgraImage {
    UINT width = 0;
    UINT height = 0;
    std::vector<BYTE> pixels;

    bool IsValid() const;
};

enum class AdjustmentPreset {
    Low,
    Balanced,
    High
};

enum class EditOperationType {
    Marker,
    Ellipse,
    Pen,
    Mosaic
};

struct EditOperation {
    EditOperationType type = EditOperationType::Marker;
    RECT rect{};
    COLORREF color = RGB(255, 59, 48);
    std::vector<POINT> points;
    int strokeWidth = 4;
};

POINT PathBreakPoint();
bool IsPathBreak(POINT point);
bool CropImage(const BgraImage& source, const RECT& region, BgraImage* output);

class ImageDocument {
public:
    explicit ImageDocument(const BgraImage& source);
    explicit ImageDocument(BgraImage&& source);

    const BgraImage& Source() const { return source_; }
    AdjustmentPreset GetAdjustmentPreset() const { return preset_; }
    void SetAdjustmentPreset(AdjustmentPreset preset) { preset_ = preset; }

    void AddOperation(const EditOperation& operation);
    bool Undo();
    bool Redo();
    void Reset();

    bool CanUndo() const { return !operations_.empty(); }
    bool CanRedo() const { return !redoOperations_.empty(); }
    size_t OperationCount() const { return operations_.size(); }
    const std::vector<EditOperation>& Operations() const { return operations_; }

    BgraImage Render() const;

private:
    BgraImage source_;
    AdjustmentPreset preset_ = AdjustmentPreset::Balanced;
    std::vector<EditOperation> operations_;
    std::vector<EditOperation> redoOperations_;
};

}  // namespace editor
