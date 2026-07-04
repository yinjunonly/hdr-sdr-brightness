using System;

internal enum CaptureLanguage
{
    English = 1,
    Chinese = 2,
    Korean = 3,
    Japanese = 4,
    Russian = 5,
    ChineseTraditional = 6,
    German = 7
}

internal enum CaptureString
{
    HiddenOwnerTitle,
    PreviewTitle,
    SaveDialogTitle,
    PngFilter,
    ToolMarkerHint,
    ToolMosaicHint,
    UndoHint,
    ResetHint,
    PresetLowHint,
    PresetBalancedHint,
    PresetHighHint,
    SaveAsFileHint,
    CancelCloseHint,
    DoneCopyCloseHint,
    DoneButton,
    PresetLow,
    PresetBalanced,
    PresetHigh,
    StatusPreset,
    MarkerModeStatus,
    MosaicModeStatus,
    ReadyStatus,
    SavedStatus,
    CopiedStatus,
    CopyFailedPrefix,
    NoUndoStatus,
    UndoneStatus,
    ResetStatus,
    AddedMarkerStatus,
    AddedMosaicStatus,
    SelectHint,
    ToolbarCancel,
    ToolbarMarker,
    ToolbarEllipse,
    ToolbarPen,
    ToolbarMosaic,
    ToolbarColor,
    ToolbarUndo,
    ToolbarRedo,
    ToolbarReset,
    ToolbarHdrLow,
    ToolbarHdrBalanced,
    ToolbarHdrHigh,
    ToolbarSave,
    ToolbarCopy,
    ProcessingStatus
}

internal static class CaptureText
{
    private static CaptureLanguage language = ResolveSystemLanguage();

    public static string FontFamily => language switch
    {
        CaptureLanguage.Chinese => "Microsoft YaHei UI",
        CaptureLanguage.ChineseTraditional => "Microsoft JhengHei UI",
        CaptureLanguage.Korean => "Malgun Gothic",
        CaptureLanguage.Japanese => "Yu Gothic UI",
        _ => "Segoe UI"
    };

    public static void Initialize(string[] args)
    {
        string? value = CaptureArgs.Value(args, "--lang");
        if (int.TryParse(value, System.Globalization.NumberStyles.Integer,
            System.Globalization.CultureInfo.InvariantCulture, out int id) &&
            Enum.IsDefined(typeof(CaptureLanguage), id))
        {
            SetLanguageId(id);
            return;
        }

        language = ResolveSystemLanguage();
    }

    public static void SetLanguageId(int id)
    {
        if (Enum.IsDefined(typeof(CaptureLanguage), id))
        {
            language = (CaptureLanguage)id;
        }
    }

    public static string Get(CaptureString id)
    {
        return language switch
        {
            CaptureLanguage.Chinese => Zh(id),
            CaptureLanguage.ChineseTraditional => ZhTw(id),
            CaptureLanguage.Korean => Ko(id),
            CaptureLanguage.Japanese => Ja(id),
            CaptureLanguage.Russian => Ru(id),
            CaptureLanguage.German => De(id),
            _ => En(id)
        };
    }

    private static CaptureLanguage ResolveSystemLanguage()
    {
        string name = System.Globalization.CultureInfo.CurrentUICulture.Name.ToLowerInvariant();
        if (name.StartsWith("zh-hant") || name is "zh-tw" or "zh-hk" or "zh-mo") return CaptureLanguage.ChineseTraditional;
        if (name.StartsWith("zh")) return CaptureLanguage.Chinese;
        if (name.StartsWith("ko")) return CaptureLanguage.Korean;
        if (name.StartsWith("ja")) return CaptureLanguage.Japanese;
        if (name.StartsWith("ru")) return CaptureLanguage.Russian;
        if (name.StartsWith("de")) return CaptureLanguage.German;
        return CaptureLanguage.English;
    }

    private static string En(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR Capture",
        CaptureString.PreviewTitle => "HDR SDR Capture Preview",
        CaptureString.SaveDialogTitle => "Save screenshot",
        CaptureString.PngFilter => "PNG image|*.png",
        CaptureString.ToolMarkerHint => "Box: drag to add a red frame",
        CaptureString.ToolMosaicHint => "Mosaic: drag to hide sensitive content",
        CaptureString.UndoHint => "Undo last step",
        CaptureString.ResetHint => "Reset edits",
        CaptureString.PresetLowHint => "Effect: Low",
        CaptureString.PresetBalancedHint => "Effect: Balanced",
        CaptureString.PresetHighHint => "Effect: High",
        CaptureString.SaveAsFileHint => "Save as file",
        CaptureString.CancelCloseHint => "Cancel and close",
        CaptureString.DoneCopyCloseHint => "Copy and close",
        CaptureString.DoneButton => "Done",
        CaptureString.PresetLow => "Low",
        CaptureString.PresetBalanced => "Balanced",
        CaptureString.PresetHigh => "High",
        CaptureString.StatusPreset => "Effect: {0}.",
        CaptureString.MarkerModeStatus => "Box mode: drag on the image to add a red frame.",
        CaptureString.MosaicModeStatus => "Mosaic mode: drag on the image to hide sensitive content.",
        CaptureString.ReadyStatus => "Copy directly, or choose a tool to annotate first.",
        CaptureString.SavedStatus => "Saved to file.",
        CaptureString.CopiedStatus => "Copied to clipboard.",
        CaptureString.CopyFailedPrefix => "Copy failed: ",
        CaptureString.NoUndoStatus => "No annotations to undo.",
        CaptureString.UndoneStatus => "Undid the last annotation.",
        CaptureString.ResetStatus => "Reset.",
        CaptureString.AddedMarkerStatus => "Added red frame. Continue annotating or click Done.",
        CaptureString.AddedMosaicStatus => "Added mosaic. Continue hiding content or click Done.",
        CaptureString.SelectHint => "Drag to select an area, Esc to cancel",
        CaptureString.ToolbarCancel => "Cancel",
        CaptureString.ToolbarMarker => "Rectangle annotation",
        CaptureString.ToolbarEllipse => "Ellipse annotation",
        CaptureString.ToolbarPen => "Pen annotation",
        CaptureString.ToolbarMosaic => "Mosaic",
        CaptureString.ToolbarColor => "Change annotation color",
        CaptureString.ToolbarUndo => "Undo (Ctrl+Z)",
        CaptureString.ToolbarRedo => "Redo (Ctrl+Y)",
        CaptureString.ToolbarReset => "Reset",
        CaptureString.ToolbarHdrLow => "HDR brightness: Low",
        CaptureString.ToolbarHdrBalanced => "HDR brightness: Balanced",
        CaptureString.ToolbarHdrHigh => "HDR brightness: High",
        CaptureString.ToolbarSave => "Save to file",
        CaptureString.ToolbarCopy => "Copy to clipboard",
        CaptureString.ProcessingStatus => "Capturing…",
        _ => string.Empty
    };

    private static string Zh(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR 截图",
        CaptureString.PreviewTitle => "HDR SDR 截图预览",
        CaptureString.SaveDialogTitle => "保存截图",
        CaptureString.PngFilter => "PNG 图片|*.png",
        CaptureString.ToolMarkerHint => "框选：拖拽添加红框",
        CaptureString.ToolMosaicHint => "马赛克：拖拽遮挡敏感区域",
        CaptureString.UndoHint => "撤销上一步",
        CaptureString.ResetHint => "重置编辑",
        CaptureString.PresetLowHint => "效果：低",
        CaptureString.PresetBalancedHint => "效果：平衡",
        CaptureString.PresetHighHint => "效果：高",
        CaptureString.SaveAsFileHint => "保存为文件",
        CaptureString.CancelCloseHint => "取消并关闭",
        CaptureString.DoneCopyCloseHint => "完成复制并关闭",
        CaptureString.DoneButton => "完成",
        CaptureString.PresetLow => "低",
        CaptureString.PresetBalanced => "平衡",
        CaptureString.PresetHigh => "高",
        CaptureString.StatusPreset => "效果：{0}。",
        CaptureString.MarkerModeStatus => "框选模式：在图片上拖拽，为重点区域添加红框。",
        CaptureString.MosaicModeStatus => "马赛克模式：在图片上拖拽，遮挡敏感内容。",
        CaptureString.ReadyStatus => "可直接完成复制，也可先选择工具进行标注。",
        CaptureString.SavedStatus => "已保存到文件。",
        CaptureString.CopiedStatus => "已复制到剪贴板。",
        CaptureString.CopyFailedPrefix => "复制失败：",
        CaptureString.NoUndoStatus => "没有可撤销的标注。",
        CaptureString.UndoneStatus => "已撤销上一步标注。",
        CaptureString.ResetStatus => "已重置。",
        CaptureString.AddedMarkerStatus => "已添加红框。可继续标注或点“完成”。",
        CaptureString.AddedMosaicStatus => "已添加马赛克。可继续遮挡或点“完成”。",
        CaptureString.SelectHint => "拖拽选择截图区域，Esc 取消",
        CaptureString.ToolbarCancel => "取消",
        CaptureString.ToolbarMarker => "矩形标注",
        CaptureString.ToolbarEllipse => "椭圆标注",
        CaptureString.ToolbarPen => "画笔标注",
        CaptureString.ToolbarMosaic => "马赛克遮挡",
        CaptureString.ToolbarColor => "切换标注颜色",
        CaptureString.ToolbarUndo => "撤销 (Ctrl+Z)",
        CaptureString.ToolbarRedo => "重做 (Ctrl+Y)",
        CaptureString.ToolbarReset => "重置",
        CaptureString.ToolbarHdrLow => "HDR 亮度：低",
        CaptureString.ToolbarHdrBalanced => "HDR 亮度：平衡",
        CaptureString.ToolbarHdrHigh => "HDR 亮度：高",
        CaptureString.ToolbarSave => "保存到文件",
        CaptureString.ToolbarCopy => "复制到剪贴板",
        CaptureString.ProcessingStatus => "正在捕获…",
        _ => string.Empty
    };

    private static string ZhTw(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR 截圖",
        CaptureString.PreviewTitle => "HDR SDR 截圖預覽",
        CaptureString.SaveDialogTitle => "儲存截圖",
        CaptureString.PngFilter => "PNG 圖片|*.png",
        CaptureString.ToolMarkerHint => "框選：拖曳加入紅框",
        CaptureString.ToolMosaicHint => "馬賽克：拖曳遮蔽敏感區域",
        CaptureString.UndoHint => "復原上一步",
        CaptureString.ResetHint => "重設編輯",
        CaptureString.PresetLowHint => "效果：低",
        CaptureString.PresetBalancedHint => "效果：平衡",
        CaptureString.PresetHighHint => "效果：高",
        CaptureString.SaveAsFileHint => "另存為檔案",
        CaptureString.CancelCloseHint => "取消並關閉",
        CaptureString.DoneCopyCloseHint => "完成複製並關閉",
        CaptureString.DoneButton => "完成",
        CaptureString.PresetLow => "低",
        CaptureString.PresetBalanced => "平衡",
        CaptureString.PresetHigh => "高",
        CaptureString.StatusPreset => "效果：{0}。",
        CaptureString.MarkerModeStatus => "框選模式：在圖片上拖曳，為重點區域加入紅框。",
        CaptureString.MosaicModeStatus => "馬賽克模式：在圖片上拖曳，遮蔽敏感內容。",
        CaptureString.ReadyStatus => "可直接完成複製，也可先選擇工具進行標註。",
        CaptureString.SavedStatus => "已儲存到檔案。",
        CaptureString.CopiedStatus => "已複製到剪貼簿。",
        CaptureString.CopyFailedPrefix => "複製失敗：",
        CaptureString.NoUndoStatus => "沒有可復原的標註。",
        CaptureString.UndoneStatus => "已復原上一步標註。",
        CaptureString.ResetStatus => "已重設。",
        CaptureString.AddedMarkerStatus => "已加入紅框。可繼續標註或按「完成」。",
        CaptureString.AddedMosaicStatus => "已加入馬賽克。可繼續遮蔽或按「完成」。",
        CaptureString.SelectHint => "拖曳選擇截圖區域，Esc 取消",
        CaptureString.ToolbarCancel => "取消",
        CaptureString.ToolbarMarker => "矩形標註",
        CaptureString.ToolbarEllipse => "橢圓標註",
        CaptureString.ToolbarPen => "畫筆標註",
        CaptureString.ToolbarMosaic => "馬賽克遮蔽",
        CaptureString.ToolbarColor => "切換標註顏色",
        CaptureString.ToolbarUndo => "復原 (Ctrl+Z)",
        CaptureString.ToolbarRedo => "重做 (Ctrl+Y)",
        CaptureString.ToolbarReset => "重設",
        CaptureString.ToolbarHdrLow => "HDR 亮度：低",
        CaptureString.ToolbarHdrBalanced => "HDR 亮度：平衡",
        CaptureString.ToolbarHdrHigh => "HDR 亮度：高",
        CaptureString.ToolbarSave => "儲存到檔案",
        CaptureString.ToolbarCopy => "複製到剪貼簿",
        CaptureString.ProcessingStatus => "正在擷取…",
        _ => string.Empty
    };

    private static string Ko(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR 캡처",
        CaptureString.PreviewTitle => "HDR SDR 캡처 미리 보기",
        CaptureString.SaveDialogTitle => "스크린샷 저장",
        CaptureString.PngFilter => "PNG 이미지|*.png",
        CaptureString.ToolMarkerHint => "상자: 끌어서 빨간 테두리 추가",
        CaptureString.ToolMosaicHint => "모자이크: 끌어서 민감한 영역 가리기",
        CaptureString.UndoHint => "마지막 단계 실행 취소",
        CaptureString.ResetHint => "편집 초기화",
        CaptureString.PresetLowHint => "효과: 낮음",
        CaptureString.PresetBalancedHint => "효과: 균형",
        CaptureString.PresetHighHint => "효과: 높음",
        CaptureString.SaveAsFileHint => "파일로 저장",
        CaptureString.CancelCloseHint => "취소하고 닫기",
        CaptureString.DoneCopyCloseHint => "복사하고 닫기",
        CaptureString.DoneButton => "완료",
        CaptureString.PresetLow => "낮음",
        CaptureString.PresetBalanced => "균형",
        CaptureString.PresetHigh => "높음",
        CaptureString.StatusPreset => "효과: {0}.",
        CaptureString.MarkerModeStatus => "상자 모드: 이미지에서 끌어 빨간 테두리를 추가합니다.",
        CaptureString.MosaicModeStatus => "모자이크 모드: 이미지에서 끌어 민감한 내용을 가립니다.",
        CaptureString.ReadyStatus => "바로 복사하거나 먼저 도구를 선택해 주석을 추가할 수 있습니다.",
        CaptureString.SavedStatus => "파일로 저장했습니다.",
        CaptureString.CopiedStatus => "클립보드에 복사했습니다.",
        CaptureString.CopyFailedPrefix => "복사 실패: ",
        CaptureString.NoUndoStatus => "실행 취소할 주석이 없습니다.",
        CaptureString.UndoneStatus => "마지막 주석을 실행 취소했습니다.",
        CaptureString.ResetStatus => "초기화했습니다.",
        CaptureString.AddedMarkerStatus => "빨간 테두리를 추가했습니다. 계속 주석을 달거나 완료를 클릭하세요.",
        CaptureString.AddedMosaicStatus => "모자이크를 추가했습니다. 계속 가리거나 완료를 클릭하세요.",
        CaptureString.SelectHint => "끌어서 영역 선택, Esc로 취소",
        CaptureString.ToolbarCancel => "취소",
        CaptureString.ToolbarMarker => "사각형 주석",
        CaptureString.ToolbarEllipse => "타원 주석",
        CaptureString.ToolbarPen => "펜 주석",
        CaptureString.ToolbarMosaic => "모자이크",
        CaptureString.ToolbarColor => "주석 색 변경",
        CaptureString.ToolbarUndo => "실행 취소 (Ctrl+Z)",
        CaptureString.ToolbarRedo => "다시 실행 (Ctrl+Y)",
        CaptureString.ToolbarReset => "초기화",
        CaptureString.ToolbarHdrLow => "HDR 밝기: 낮음",
        CaptureString.ToolbarHdrBalanced => "HDR 밝기: 균형",
        CaptureString.ToolbarHdrHigh => "HDR 밝기: 높음",
        CaptureString.ToolbarSave => "파일로 저장",
        CaptureString.ToolbarCopy => "클립보드에 복사",
        CaptureString.ProcessingStatus => "캡처 중…",
        _ => string.Empty
    };

    private static string Ja(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR キャプチャ",
        CaptureString.PreviewTitle => "HDR SDR キャプチャ プレビュー",
        CaptureString.SaveDialogTitle => "スクリーンショットを保存",
        CaptureString.PngFilter => "PNG 画像|*.png",
        CaptureString.ToolMarkerHint => "枠: ドラッグして赤枠を追加",
        CaptureString.ToolMosaicHint => "モザイク: ドラッグして機密部分を隠す",
        CaptureString.UndoHint => "前の操作を元に戻す",
        CaptureString.ResetHint => "編集をリセット",
        CaptureString.PresetLowHint => "効果: 低",
        CaptureString.PresetBalancedHint => "効果: バランス",
        CaptureString.PresetHighHint => "効果: 高",
        CaptureString.SaveAsFileHint => "ファイルとして保存",
        CaptureString.CancelCloseHint => "キャンセルして閉じる",
        CaptureString.DoneCopyCloseHint => "コピーして閉じる",
        CaptureString.DoneButton => "完了",
        CaptureString.PresetLow => "低",
        CaptureString.PresetBalanced => "バランス",
        CaptureString.PresetHigh => "高",
        CaptureString.StatusPreset => "効果: {0}。",
        CaptureString.MarkerModeStatus => "枠モード: 画像上でドラッグして赤枠を追加します。",
        CaptureString.MosaicModeStatus => "モザイクモード: 画像上でドラッグして機密内容を隠します。",
        CaptureString.ReadyStatus => "そのままコピーするか、先にツールを選んで注釈を追加できます。",
        CaptureString.SavedStatus => "ファイルに保存しました。",
        CaptureString.CopiedStatus => "クリップボードにコピーしました。",
        CaptureString.CopyFailedPrefix => "コピー失敗: ",
        CaptureString.NoUndoStatus => "元に戻せる注釈はありません。",
        CaptureString.UndoneStatus => "前の注釈を元に戻しました。",
        CaptureString.ResetStatus => "リセットしました。",
        CaptureString.AddedMarkerStatus => "赤枠を追加しました。続けて注釈するか、完了をクリックしてください。",
        CaptureString.AddedMosaicStatus => "モザイクを追加しました。続けて隠すか、完了をクリックしてください。",
        CaptureString.SelectHint => "ドラッグして範囲を選択、Esc でキャンセル",
        CaptureString.ToolbarCancel => "キャンセル",
        CaptureString.ToolbarMarker => "矩形注釈",
        CaptureString.ToolbarEllipse => "楕円注釈",
        CaptureString.ToolbarPen => "ペン注釈",
        CaptureString.ToolbarMosaic => "モザイク",
        CaptureString.ToolbarColor => "注釈の色を変更",
        CaptureString.ToolbarUndo => "元に戻す (Ctrl+Z)",
        CaptureString.ToolbarRedo => "やり直し (Ctrl+Y)",
        CaptureString.ToolbarReset => "リセット",
        CaptureString.ToolbarHdrLow => "HDR 明るさ: 低",
        CaptureString.ToolbarHdrBalanced => "HDR 明るさ: バランス",
        CaptureString.ToolbarHdrHigh => "HDR 明るさ: 高",
        CaptureString.ToolbarSave => "ファイルに保存",
        CaptureString.ToolbarCopy => "クリップボードにコピー",
        CaptureString.ProcessingStatus => "キャプチャ中…",
        _ => string.Empty
    };

    private static string Ru(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR Capture",
        CaptureString.PreviewTitle => "Предпросмотр HDR SDR",
        CaptureString.SaveDialogTitle => "Сохранить снимок",
        CaptureString.PngFilter => "PNG-изображение|*.png",
        CaptureString.ToolMarkerHint => "Рамка: перетащите, чтобы добавить красную рамку",
        CaptureString.ToolMosaicHint => "Мозаика: перетащите, чтобы скрыть важную область",
        CaptureString.UndoHint => "Отменить последний шаг",
        CaptureString.ResetHint => "Сбросить правки",
        CaptureString.PresetLowHint => "Эффект: низкий",
        CaptureString.PresetBalancedHint => "Эффект: сбалансированный",
        CaptureString.PresetHighHint => "Эффект: высокий",
        CaptureString.SaveAsFileHint => "Сохранить как файл",
        CaptureString.CancelCloseHint => "Отменить и закрыть",
        CaptureString.DoneCopyCloseHint => "Скопировать и закрыть",
        CaptureString.DoneButton => "Готово",
        CaptureString.PresetLow => "Низкий",
        CaptureString.PresetBalanced => "Баланс",
        CaptureString.PresetHigh => "Высокий",
        CaptureString.StatusPreset => "Эффект: {0}.",
        CaptureString.MarkerModeStatus => "Режим рамки: перетащите по изображению, чтобы добавить красную рамку.",
        CaptureString.MosaicModeStatus => "Режим мозаики: перетащите по изображению, чтобы скрыть важное содержимое.",
        CaptureString.ReadyStatus => "Можно сразу скопировать или сначала выбрать инструмент для пометок.",
        CaptureString.SavedStatus => "Сохранено в файл.",
        CaptureString.CopiedStatus => "Скопировано в буфер обмена.",
        CaptureString.CopyFailedPrefix => "Не удалось скопировать: ",
        CaptureString.NoUndoStatus => "Нет пометок для отмены.",
        CaptureString.UndoneStatus => "Последняя пометка отменена.",
        CaptureString.ResetStatus => "Сброшено.",
        CaptureString.AddedMarkerStatus => "Красная рамка добавлена. Продолжайте пометки или нажмите «Готово».",
        CaptureString.AddedMosaicStatus => "Мозаика добавлена. Продолжайте скрывать содержимое или нажмите «Готово».",
        CaptureString.SelectHint => "Перетащите, чтобы выбрать область; Esc — отмена",
        CaptureString.ToolbarCancel => "Отмена",
        CaptureString.ToolbarMarker => "Прямоугольник",
        CaptureString.ToolbarEllipse => "Эллипс",
        CaptureString.ToolbarPen => "Перо",
        CaptureString.ToolbarMosaic => "Мозаика",
        CaptureString.ToolbarColor => "Изменить цвет пометки",
        CaptureString.ToolbarUndo => "Отменить (Ctrl+Z)",
        CaptureString.ToolbarRedo => "Повторить (Ctrl+Y)",
        CaptureString.ToolbarReset => "Сброс",
        CaptureString.ToolbarHdrLow => "Яркость HDR: низкая",
        CaptureString.ToolbarHdrBalanced => "Яркость HDR: баланс",
        CaptureString.ToolbarHdrHigh => "Яркость HDR: высокая",
        CaptureString.ToolbarSave => "Сохранить в файл",
        CaptureString.ToolbarCopy => "Копировать в буфер обмена",
        CaptureString.ProcessingStatus => "Захват…",
        _ => string.Empty
    };

    private static string De(CaptureString id) => id switch
    {
        CaptureString.HiddenOwnerTitle => "HDR SDR Aufnahme",
        CaptureString.PreviewTitle => "HDR SDR Aufnahmevorschau",
        CaptureString.SaveDialogTitle => "Screenshot speichern",
        CaptureString.PngFilter => "PNG-Bild|*.png",
        CaptureString.ToolMarkerHint => "Rahmen: ziehen, um einen roten Rahmen hinzuzufügen",
        CaptureString.ToolMosaicHint => "Mosaik: ziehen, um sensible Bereiche zu verdecken",
        CaptureString.UndoHint => "Letzten Schritt rückgängig machen",
        CaptureString.ResetHint => "Bearbeitungen zurücksetzen",
        CaptureString.PresetLowHint => "Effekt: Niedrig",
        CaptureString.PresetBalancedHint => "Effekt: Ausgewogen",
        CaptureString.PresetHighHint => "Effekt: Hoch",
        CaptureString.SaveAsFileHint => "Als Datei speichern",
        CaptureString.CancelCloseHint => "Abbrechen und schließen",
        CaptureString.DoneCopyCloseHint => "Kopieren und schließen",
        CaptureString.DoneButton => "Fertig",
        CaptureString.PresetLow => "Niedrig",
        CaptureString.PresetBalanced => "Ausgewogen",
        CaptureString.PresetHigh => "Hoch",
        CaptureString.StatusPreset => "Effekt: {0}.",
        CaptureString.MarkerModeStatus => "Rahmenmodus: Auf dem Bild ziehen, um einen roten Rahmen hinzuzufügen.",
        CaptureString.MosaicModeStatus => "Mosaikmodus: Auf dem Bild ziehen, um sensible Inhalte zu verdecken.",
        CaptureString.ReadyStatus => "Direkt kopieren oder zuerst ein Werkzeug zum Markieren wählen.",
        CaptureString.SavedStatus => "In Datei gespeichert.",
        CaptureString.CopiedStatus => "In die Zwischenablage kopiert.",
        CaptureString.CopyFailedPrefix => "Kopieren fehlgeschlagen: ",
        CaptureString.NoUndoStatus => "Keine Markierung zum Rückgängigmachen.",
        CaptureString.UndoneStatus => "Letzte Markierung rückgängig gemacht.",
        CaptureString.ResetStatus => "Zurückgesetzt.",
        CaptureString.AddedMarkerStatus => "Roter Rahmen hinzugefügt. Weiter markieren oder auf Fertig klicken.",
        CaptureString.AddedMosaicStatus => "Mosaik hinzugefügt. Weiter verdecken oder auf Fertig klicken.",
        CaptureString.SelectHint => "Ziehen, um einen Bereich auszuwählen; Esc zum Abbrechen",
        CaptureString.ToolbarCancel => "Abbrechen",
        CaptureString.ToolbarMarker => "Rechteckmarkierung",
        CaptureString.ToolbarEllipse => "Ellipsenmarkierung",
        CaptureString.ToolbarPen => "Stiftmarkierung",
        CaptureString.ToolbarMosaic => "Mosaik",
        CaptureString.ToolbarColor => "Markierungsfarbe ändern",
        CaptureString.ToolbarUndo => "Rückgängig (Ctrl+Z)",
        CaptureString.ToolbarRedo => "Wiederholen (Ctrl+Y)",
        CaptureString.ToolbarReset => "Zurücksetzen",
        CaptureString.ToolbarHdrLow => "HDR-Helligkeit: niedrig",
        CaptureString.ToolbarHdrBalanced => "HDR-Helligkeit: ausgewogen",
        CaptureString.ToolbarHdrHigh => "HDR-Helligkeit: hoch",
        CaptureString.ToolbarSave => "In Datei speichern",
        CaptureString.ToolbarCopy => "In Zwischenablage kopieren",
        CaptureString.ProcessingStatus => "Erfasse…",
        _ => string.Empty
    };
}


