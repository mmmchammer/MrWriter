#include "markdownbox.h"

MarkdownBox::MarkdownBox(QWidget *parent)
    : AbstractTextBox(parent) {}

void MarkdownBox::applyText(){
    hide();
    setText(QString(""));
    emit updatePageMarkdown(m_pageNum);
}
