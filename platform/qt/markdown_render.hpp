// AriaAgent — lightweight Markdown → HTML renderer with code highlighting.
//
// Ported concept from harness ui-conversation: assistant messages are
// rendered as rich text. This is a small self-contained Markdown subset
// renderer (headings / bold / italic / inline-code / fenced code blocks /
// lists / blockquotes / links) with a simple syntax highlighter for code
// fences (keywords, strings, numbers, comments).
#pragma once

#include <QString>
#include <QTextDocument>

namespace agent_ui {

// Render `markdown` into `doc` (the caller owns the document).
// `lang_hint` optionally forces a language for bare code blocks.
void render_markdown(const QString& markdown, QTextDocument& doc);

// HTML fragment of the rendered markdown (for debugging / tooling).
QString markdown_to_html(const QString& markdown);

} // namespace agent_ui
