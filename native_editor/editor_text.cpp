#include "editor_text.h"

#include <cstddef>

namespace editor {

namespace {

struct EditorStrings {
    const wchar_t* values[static_cast<size_t>(EditorTextId::Count)];
    const wchar_t* font;
};

const EditorStrings kEnglish{{
    L"HDR SDR Capture Preview",
    L"HDR SDR Region Selection",
    L"Drag to select an area, Esc to cancel",
    L"Save screenshot",
    L"Copied to clipboard.",
    L"Saved to file.",
    L"Cancel",
    L"Rectangle annotation",
    L"Ellipse annotation",
    L"Pen annotation",
    L"Mosaic",
    L"Change annotation color",
    L"Undo (Ctrl+Z)",
    L"Redo (Ctrl+Y)",
    L"Reset",
    L"HDR brightness: Low",
    L"HDR brightness: Balanced",
    L"HDR brightness: High",
    L"Save to file",
    L"Copy to clipboard"
}, L"Segoe UI"};

const EditorStrings kChinese{{
    L"HDR SDR 截图预览", L"HDR SDR 区域截图", L"拖拽选择截图区域，Esc 取消", L"保存截图",
    L"已复制到剪贴板。", L"已保存到文件。", L"取消", L"矩形标注", L"椭圆标注", L"画笔标注",
    L"马赛克遮挡", L"切换标注颜色", L"撤销 (Ctrl+Z)", L"重做 (Ctrl+Y)", L"重置",
    L"HDR 亮度：低", L"HDR 亮度：平衡", L"HDR 亮度：高", L"保存到文件", L"复制到剪贴板"
}, L"Microsoft YaHei UI"};

const EditorStrings kKorean{{
    L"HDR SDR 캡처 미리 보기", L"HDR SDR 영역 캡처", L"끌어서 영역 선택, Esc로 취소", L"스크린샷 저장",
    L"클립보드에 복사했습니다.", L"파일로 저장했습니다.", L"취소", L"사각형 주석", L"타원 주석", L"펜 주석",
    L"모자이크", L"주석 색 변경", L"실행 취소 (Ctrl+Z)", L"다시 실행 (Ctrl+Y)", L"초기화",
    L"HDR 밝기: 낮음", L"HDR 밝기: 균형", L"HDR 밝기: 높음", L"파일로 저장", L"클립보드에 복사"
}, L"Malgun Gothic"};

const EditorStrings kJapanese{{
    L"HDR SDR キャプチャ プレビュー", L"HDR SDR 範囲キャプチャ", L"ドラッグして範囲を選択、Esc でキャンセル", L"スクリーンショットを保存",
    L"クリップボードにコピーしました。", L"ファイルに保存しました。", L"キャンセル", L"矩形注釈", L"楕円注釈", L"ペン注釈",
    L"モザイク", L"注釈の色を変更", L"元に戻す (Ctrl+Z)", L"やり直し (Ctrl+Y)", L"リセット",
    L"HDR 明るさ: 低", L"HDR 明るさ: バランス", L"HDR 明るさ: 高", L"ファイルに保存", L"クリップボードにコピー"
}, L"Yu Gothic UI"};

const EditorStrings kRussian{{
    L"Предпросмотр HDR SDR", L"Выбор области HDR SDR", L"Перетащите, чтобы выбрать область; Esc — отмена", L"Сохранить снимок",
    L"Скопировано в буфер обмена.", L"Сохранено в файл.", L"Отмена", L"Прямоугольник", L"Эллипс", L"Перо",
    L"Мозаика", L"Изменить цвет пометки", L"Отменить (Ctrl+Z)", L"Повторить (Ctrl+Y)", L"Сброс",
    L"Яркость HDR: низкая", L"Яркость HDR: баланс", L"Яркость HDR: высокая", L"Сохранить в файл", L"Копировать в буфер обмена"
}, L"Segoe UI"};

const EditorStrings kTraditionalChinese{{
    L"HDR SDR 截圖預覽", L"HDR SDR 區域截圖", L"拖曳選擇截圖區域，Esc 取消", L"儲存截圖",
    L"已複製到剪貼簿。", L"已儲存到檔案。", L"取消", L"矩形標註", L"橢圓標註", L"畫筆標註",
    L"馬賽克遮蔽", L"切換標註顏色", L"復原 (Ctrl+Z)", L"重做 (Ctrl+Y)", L"重設",
    L"HDR 亮度：低", L"HDR 亮度：平衡", L"HDR 亮度：高", L"儲存到檔案", L"複製到剪貼簿"
}, L"Microsoft JhengHei UI"};

const EditorStrings kGerman{{
    L"HDR SDR Aufnahmevorschau", L"HDR SDR Bereichsauswahl", L"Ziehen, um einen Bereich auszuwählen; Esc zum Abbrechen", L"Screenshot speichern",
    L"In die Zwischenablage kopiert.", L"In Datei gespeichert.", L"Abbrechen", L"Rechteckmarkierung", L"Ellipsenmarkierung", L"Stiftmarkierung",
    L"Mosaik", L"Markierungsfarbe ändern", L"Rückgängig (Ctrl+Z)", L"Wiederholen (Ctrl+Y)", L"Zurücksetzen",
    L"HDR-Helligkeit: niedrig", L"HDR-Helligkeit: ausgewogen", L"HDR-Helligkeit: hoch", L"In Datei speichern", L"In Zwischenablage kopieren"
}, L"Segoe UI"};

const EditorStrings& StringsFor(int language) {
    switch (language) {
    case 2: return kChinese;
    case 3: return kKorean;
    case 4: return kJapanese;
    case 5: return kRussian;
    case 6: return kTraditionalChinese;
    case 7: return kGerman;
    default: return kEnglish;
    }
}

}  // namespace

const wchar_t* GetEditorText(int language, EditorTextId id) {
    size_t index = static_cast<size_t>(id);
    if (index >= static_cast<size_t>(EditorTextId::Count)) return L"";
    return StringsFor(language).values[index];
}

const wchar_t* GetEditorFontName(int language) {
    return StringsFor(language).font;
}

}  // namespace editor
