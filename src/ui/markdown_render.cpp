// AriaAgent — Markdown renderer implementation.
#include "ui/markdown_render.hpp"

#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextFrameFormat>

#include <functional>
#include <vector>

namespace agent_ui {

namespace {

QString escape_html(const QString& s) {
    QString out = s;
    out.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    return out;
}

// ── Minimal syntax highlighter (C++/JS/TS/py-ish) ──────────────────────────
QString highlight_code(const QString& code) {
    static const QRegularExpression re(
        R"((\b(?:return|if|else|for|while|break|continue|class|struct|enum|public|private|protected|static|const|constexpr|auto|void|int|float|double|bool|string|char|template|typename|namespace|using|new|delete|true|false|nullptr|this|virtual|override|co_await|co_return|async|await|function|let|var|import|export|from|def|lambda|std|require|yield)\b)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b\d+(?:\.\d+)?\b)|(//[^\n]*|#[^\n]*|/\*[\s\S]*?\*/))");
    const QString ckw = "#c678dd", cstr = "#98c379", cnum = "#d19a66", ccmt = "#5c6370";
    QString out = escape_html(code);
    // Multi-line comments first (can't regex across our escaped string easily,
    // so pre-split by lines for simplicity).
    QStringList lines = out.split('\n');
    bool in_block = false;
    for (QString& line : lines) {
        // handle // comments
        qsizetype ci = line.indexOf("//");
        if (ci >= 0) {
            QString pre = line.left(ci), cmt = line.mid(ci);
            line = pre + QStringLiteral("<span style='color:%1'>%2</span>").arg(ccmt, escape_html(cmt));
        }
        // block comment state (crude)
        (void)in_block;
    }
    out = lines.join('\n');

    auto wrap = [&](const QString& s, const QString& color) {
        return QStringLiteral("<span style='color:%1'>%2</span>").arg(color, s);
    };
    QString result;
    QRegularExpressionMatchIterator it = re.globalMatch(out);
    qsizetype last = 0;
    while (it.hasNext()) {
        auto m = it.next();
        result += out.mid(last, m.capturedStart() - last);
        if (!m.captured(1).isEmpty()) result += wrap(m.captured(1), ckw);
        else if (!m.captured(2).isEmpty()) result += wrap(m.captured(2), cstr);
        else if (!m.captured(3).isEmpty()) result += wrap(m.captured(3), cnum);
        else result += wrap(m.captured(4), ccmt);
        last = m.capturedEnd();
    }
    result += out.mid(last);
    return result;
}

QString render_inline(const QString& text) {
    QString out = escape_html(text);
    // `code`
    out.replace(QRegularExpression(R"(`([^`]+)`)"),
                QStringLiteral("<span style='background:#2a2f3a; color:#e06c75; font-family:Consolas,monospace; padding:0 3px; border-radius:3px;'>\\1</span>"));
    // **bold**
    out.replace(QRegularExpression(R"(\*\*([^*]+)\*\*)"), QStringLiteral("<b>\\1</b>"));
    // *italic*
    out.replace(QRegularExpression(R"(\*([^*]+)\*)"), QStringLiteral("<i>\\1</i>"));
    // [text](url)
    out.replace(QRegularExpression(R"(\[([^\]]+)\]\(([^)]+)\))"),
                QStringLiteral("<a style='color:#3b82f6;' href='\\2'>\\1</a>"));
    return out;
}

} // namespace

QString markdown_to_html(const QString& markdown) {
    QString html;
    const QStringList lines = markdown.split('\n');
    bool in_code = false;
    QString code_buf;
    QString code_lang;
    bool in_list = false;
    bool in_quote = false;

    auto flush_code = [&] {
        if (!in_code) return;
        QString highlighted = highlight_code(code_buf);
        html += QStringLiteral("<pre style='background:#161a22; border-radius:8px; padding:10px; margin:6px 0;'><code style='font-family:Consolas,monospace; color:#abb2bf;'>%1</code></pre>")
                    .arg(highlighted);
        code_buf.clear();
        code_lang.clear();
        in_code = false;
    };

    for (const QString& raw : lines) {
        QString line = raw;
        if (line.endsWith('\r')) line.chop(1);

        // Fenced code block
        QRegularExpression fence(R"(^```(.*)$)");
        auto fm = fence.match(line);
        if (fm.hasMatch()) {
            if (in_code) flush_code();
            else { in_code = true; code_lang = fm.captured(1).trimmed(); }
            continue;
        }
        if (in_code) { code_buf += line + "\n"; continue; }

        // Blank line closes lists/quotes
        if (line.trimmed().isEmpty()) {
            if (in_list) { html += "</ul>"; in_list = false; }
            if (in_quote) { html += "</blockquote>"; in_quote = false; }
            continue;
        }

        // Headings
        for (int h = 1; h <= 4; ++h) {
            QString prefix(h, '#');
            if (line.startsWith(prefix + " ")) {
                if (in_list) { html += "</ul>"; in_list = false; }
                if (in_quote) { html += "</blockquote>"; in_quote = false; }
                html += QStringLiteral("<h%1>%2</h%1>").arg(h).arg(render_inline(line.mid(h + 1)));
                goto next_line;
            }
        }

        // Blockquote
        if (line.startsWith("> ")) {
            if (!in_quote) { html += "<blockquote style='border-left:3px solid #3b82f6; margin:6px 0; padding-left:10px; color:#8b93a3;'>"; in_quote = true; }
            html += render_inline(line.mid(2)) + "<br/>";
            goto next_line;
        }

        // List item
        {
            QRegularExpression item(R"(^\s*[-*+]\s+(.*)$)");
            auto im = item.match(line);
            if (im.hasMatch()) {
                if (!in_list) { html += "<ul style='margin:6px 0; padding-left:20px;'>"; in_list = true; }
                html += "<li>" + render_inline(im.captured(1)) + "</li>";
                goto next_line;
            }
            QRegularExpression numitem(R"(^\s*\d+\.\s+(.*)$)");
            auto nm = numitem.match(line);
            if (nm.hasMatch()) {
                if (!in_list) { html += "<ol style='margin:6px 0; padding-left:20px;'>"; in_list = true; }
                html += "<li>" + render_inline(nm.captured(1)) + "</li>";
                goto next_line;
            }
        }

        // Horizontal rule
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr style='border:none; border-top:1px solid #2a2f3a; margin:8px 0;'/>";
            goto next_line;
        }

        // Plain paragraph
        if (in_list) { html += "</ul>"; in_list = false; }
        if (in_quote) { html += "</blockquote>"; in_quote = false; }
        html += "<p>" + render_inline(line) + "</p>";

        next_line:;
    }
    flush_code();
    if (in_list) html += "</ul>";
    if (in_quote) html += "</blockquote>";

    return html;
}

void render_markdown(const QString& markdown, QTextDocument& doc) {
    const QString html = markdown_to_html(markdown);
    const QString styled = QStringLiteral(
        "<style>"
        "body { font-size:14px; }"
        "p { margin:4px 0; }"
        "h1,h2,h3,h4 { margin:8px 0 4px 0; color:#f1f5f9; }"
        "a { color:#3b82f6; text-decoration:none; }"
        "</style>%1").arg(html);
    doc.setHtml(styled);
    doc.setDefaultStyleSheet(QStringLiteral(
        "body { color:#e5e7eb; font-size:14px; font-family:'Segoe UI','Microsoft YaHei UI',sans-serif; }"));
}

} // namespace agent_ui
