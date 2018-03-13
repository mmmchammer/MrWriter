#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "page.h"
#include <poppler-qt5.h>
#include <memory>

#include <QVector>

namespace MrDoc
{

class Document
{
public:
  Document();
  /*Document(const Document &doc);

  Document& operator= (const Document& doc);*/

  void exportPDF(QString fileName);

  bool loadXOJ(QString fileName);
  bool saveXOJ(QString fileName);

  bool loadMOJ(QString fileName);
  bool saveMOJ(QString fileName);

  bool loadPDF(QString fileName);

  void paintPage(int pageNum, QPainter &painter, qreal zoom);

  bool setDocName(QString docName);
  QString docName();

  bool setPath(QString path);
  QString path();

  bool documentChanged();
  void setDocumentChanged(bool changed);

  QVector<MrDoc::Page> pages;

  QString toRGBA(QString argb);
  QString toARGB(QString rgba);

  QColor stringToColor(QString colorString);

private:
  bool m_documentChanged;

  QString m_docName;
  QString m_path;
  QString m_pdfPath;

  std::shared_ptr<Poppler::Document> m_pdfDoc;
};
}

#endif // DOCUMENT_H
