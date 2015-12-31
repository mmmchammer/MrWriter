/*
#####################################################################
Copyright (C) 2015 Thomas Leitz (thomas.leitz@web.de)
#####################################################################

LICENSE:

This file is part of MrWriter.

MrWriter is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License 2.0 as published
by the Free Software Foundation.

MrWriter is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MrWriter.  If not, see <http://www.gnu.org/licenses/>.
#####################################################################
*/

#include "document.h"

#include "qcompressor.h"
#include "version.h"

#include <QPdfWriter>
#include <QPrinter>
#include <QPageSize>
#include <iostream>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QErrorMessage>
#include <QSvgGenerator>
#include <QDebug>

#include <zlib.h>

// static members

namespace MrDoc {



    Document::Document()
    {
        for (int i = 0; i < 1; ++i)
        {
            Page nextPage;
            pages.append(nextPage);
        }
        setDocumentChanged(false);
    }

    Document::Document(const Document& doc)
    {
        for (int i = 0; i < doc.pages.size(); ++i)
        {
            pages.append(doc.pages.at(i));
        }
    }

    void Document::paintPage(int pageNum, QPainter &painter, qreal zoom)
    {
        pages[pageNum].paint(painter, zoom);
    }

    /* will become exportSVG
    void Document::exportPDF(QString fileName)
    {
        QPainter painter;
        QSvgGenerator svgGenerator;
        for (int pageNum = 0; pageNum < pages.size(); ++pageNum)
        {
            svgGenerator.setFileName(fileName); //.prepend(QString::number(pageNum)));
            svgGenerator.setSize(QSize(pages[pageNum].getWidth(), pages[pageNum].getHeight()));
    //        svgGenerator.setViewBox(QRect(0, 0, pages[pageNum+1].getWidth(), pages[pageNum+1].getHeight()));
            svgGenerator.setResolution(72);
            if (!painter.begin(&svgGenerator))
            {
                qDebug() << "error";
            }

            pages[pageNum].paint(painter, 1.0);
            painter.end();
        }
    }
    */

    void Document::exportPDF(QString fileName)
    {
    //    QPdfWriter pdfWriter("/Users/tom/Desktop/qpdfwriter.pdf");
    //    QPdfWriter pdfWriter(fileName);

        QPrinter pdfWriter(QPrinter::HighResolution);
        pdfWriter.setOutputFormat(QPrinter::PdfFormat);
        pdfWriter.setOutputFileName(fileName);
    //    pdfWriter.setMargins();

        pdfWriter.setPageSize(QPageSize(QSizeF(pages[0].getWidth(), pages[0].getHeight()), QPageSize::Point));
        pdfWriter.setPageMargins(QMarginsF(0,0,0,0));
        qreal zoomW =  ((qreal)pdfWriter.pageRect().width()) / ((qreal)pdfWriter.paperRect().width());
        qreal zoomH =  ((qreal)pdfWriter.pageRect().height()) / ((qreal)pdfWriter.paperRect().height());
        qreal zoom = zoomW;
        if (zoomH < zoomW)
            zoom = zoomH;
        pdfWriter.setResolution(72);
        pdfWriter.pageLayout().setUnits(QPageLayout::Point);
        QPainter painter;

    //    std::cout << "PDF " << pdfWriter.colorCount() << std::endl;

        painter.begin(&pdfWriter);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int pageNum = 0; pageNum < pages.size(); ++pageNum)
        {
            if (pages[pageNum].backgroundColor != QColor("white"))
            {
                QRectF pageRect = pdfWriter.pageRect(QPrinter::Point);
                pageRect.translate(-pageRect.topLeft());
                painter.fillRect(pageRect, pages[pageNum].backgroundColor);
            }
            pages[pageNum].paint(painter, zoom, QRect(0,0,0,0));

            if (pageNum+1 < pages.size())
            {
                pdfWriter.setPageSize(QPageSize(QSize(pages[pageNum+1].getWidth(), pages[pageNum+1].getHeight())));
                pdfWriter.newPage();
            }
        }
        painter.end();
    }

    bool Document::loadXOJ(QString fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }

        QXmlStreamReader reader;

        // check if it is a gzipped xoj
        QByteArray s = file.read(2);
        if (s.size() == 2)
        {
            if (s.at(0) == static_cast<char>(0x1f) && s.at(1) == static_cast<char>(0x8b))
            {
                // this is a gzipped file
                file.reset();
                QByteArray compressedData = file.readAll();
                QByteArray uncompressedData;
                if (!QCompressor::gzipDecompress(compressedData, uncompressedData))
                {
                    return false;
                }
                reader.addData(uncompressedData);
            } else {
                file.reset();
                reader.setDevice(&file);
            }
        } else {
            return false;
        }

        pages.clear();

        int strokeCount = 0;

        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.name() == "page" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef width = attributes.value("", "width");
                QStringRef height = attributes.value("", "height");
                Page newPage;
                newPage.setWidth(width.toDouble());
                newPage.setHeight(height.toDouble());

                pages.append(newPage);
            }
            if(reader.name() == "background" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef color = attributes.value("", "color");
                QColor newColor = stringToColor(color.toString());
                pages.last().backgroundColor = newColor;
            }
            if (reader.name() == "stroke" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef tool = attributes.value("", "tool");
                if (tool == "pen")
                {
                    Stroke newStroke;
                    newStroke.pattern = MrDoc::solidLinePattern;
                    QStringRef color = attributes.value("", "color");
                    newStroke.color = stringToColor(color.toString());
                    QStringRef strokeWidth = attributes.value("", "width");
                    QStringList strokeWidthList = strokeWidth.toString().split(" ");
                    newStroke.penWidth = strokeWidthList.at(0).toDouble();
                    newStroke.pressures.append(newStroke.penWidth / strokeWidthList.at(0).toDouble());
                    for (int i = 1; i < strokeWidthList.size(); ++i)
                    {
                        newStroke.pressures.append(2 * strokeWidthList.at(i).toDouble() / newStroke.penWidth - newStroke.pressures.at(i-1));
                    }
                    QString elementText = reader.readElementText();
                    QStringList elementTextList = elementText.split(" ");
                    for (int i = 0; i+1 < elementTextList.size(); i = i + 2)
                    {
                        newStroke.points.append(QPointF(elementTextList.at(i).toDouble(), elementTextList.at(i+1).toDouble()));
                    }
                    while (newStroke.points.size() > newStroke.pressures.size())
                    {
                        newStroke.pressures.append(1.0);
                    }
                    pages.last().m_strokes.append(newStroke);
                    strokeCount++;
                    qDebug() << strokeCount;
                }
            }
        }

    //    QFileInfo fileInfo(file);
        file.close();

        if (reader.hasError())
        {
            return false;
        } else {
    //        path = fileInfo.absolutePath();
    //        docName = fileInfo.completeBaseName();
            setDocumentChanged(true);
            return true;
        }

    }

    bool Document::saveXOJ(QString fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }

        QXmlStreamWriter writer;

        writer.setAutoFormatting(true);

        writer.setDevice(&file);

        writer.writeStartDocument("1.0", false);
        writer.writeStartElement("xournal");
        writer.writeAttribute(QXmlStreamAttribute("version", "0.4.8"));

        writer.writeStartElement("title");
        writer.writeCharacters("Xournal document - see http://math.mit.edu/~auroux/software/xournal/");
        writer.writeEndElement();

        for (int i = 0; i < pages.size(); ++i)
        {
            writer.writeStartElement("page");
            writer.writeAttribute(QXmlStreamAttribute("width" , QString::number(pages[i].getWidth())));
            writer.writeAttribute(QXmlStreamAttribute("height", QString::number(pages[i].getHeight())));
            writer.writeEmptyElement("background");
            writer.writeAttribute(QXmlStreamAttribute("type", "solid"));
            writer.writeAttribute(QXmlStreamAttribute("color", toRGBA(pages[i].backgroundColor.name(QColor::HexArgb))));
            writer.writeAttribute(QXmlStreamAttribute("style", "plain"));
            writer.writeStartElement("layer");

            for (int j = 0; j < pages[i].m_strokes.size(); ++j)
            {
                writer.writeStartElement("stroke");
                writer.writeAttribute(QXmlStreamAttribute("tool", "pen"));
                writer.writeAttribute(QXmlStreamAttribute("color", toRGBA(pages[i].m_strokes[j].color.name(QColor::HexArgb))));
                qreal width = pages[i].m_strokes[j].penWidth;
                QString widthString;
                widthString.append(QString::number(width));
                for (int k = 0; k < pages[i].m_strokes[j].pressures.size()-1; ++k)
                {
                    qreal p0 = pages[i].m_strokes[j].pressures[k];
                    qreal p1 = pages[i].m_strokes[j].pressures[k+1];
                    widthString.append(' ');
                    widthString.append(QString::number(0.5 * (p0+p1) * width));
                }
                writer.writeAttribute(QXmlStreamAttribute("width", widthString));
                for (int k = 0; k < pages[i].m_strokes[j].points.size(); ++k)
                {
                    writer.writeCharacters(QString::number(pages[i].m_strokes[j].points[k].x()));
                    writer.writeCharacters(" ");
                    writer.writeCharacters(QString::number(pages[i].m_strokes[j].points[k].y()));
                    writer.writeCharacters(" ");
                }
                writer.writeEndElement(); // closing "stroke"
            }

            writer.writeEndElement(); // closing "layer"
            writer.writeEndElement(); // closing "page"
        }

        writer.writeEndDocument();

        QFileInfo fileInfo(file);

        file.close();

        if (writer.hasError())
        {
            return false;
        } else {
    //        setDocumentChanged(false);
    //        path = fileInfo.absolutePath();
    //        docName = fileInfo.completeBaseName();
            return true;
        }

    }

    bool Document::loadMOJ(QString fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
        {
            return false;
        }

        QXmlStreamReader reader;

        // check if it is a gzipped moj
        QByteArray s = file.read(2);
        if (s.size() == 2)
        {
            if (s.at(0) == static_cast<char>(0x1f) && s.at(1) == static_cast<char>(0x8b))
            {
                // this is a gzipped file
                file.reset();
                QByteArray compressedData = file.readAll();
                QByteArray uncompressedData;
                if (!QCompressor::gzipDecompress(compressedData, uncompressedData))
                {
                    return false;
                }
                reader.addData(uncompressedData);
            } else {
                file.reset();
                reader.setDevice(&file);
            }
        } else {
            return false;
        }

        pages.clear();

        int strokeCount = 0;

        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.name() == "MrWriter" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef docversion = attributes.value("document-version");
                if (docversion.toInt() > MY_DOC_VERSION)
                {
                    // TODO warn about newer document version
                }
            }
            if (reader.name() == "page" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef width = attributes.value("", "width");
                QStringRef height = attributes.value("", "height");
                Page newPage;
                newPage.setWidth(width.toDouble());
                newPage.setHeight(height.toDouble());

                pages.append(newPage);
            }
            if(reader.name() == "background" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef color = attributes.value("", "color");
                QColor newColor = stringToColor(color.toString());
                pages.last().backgroundColor = newColor;
            }
            if (reader.name() == "stroke" && reader.tokenType() == QXmlStreamReader::StartElement)
            {
                QXmlStreamAttributes attributes = reader.attributes();
                QStringRef tool = attributes.value("", "tool");
                if (tool == "pen")
                {
                    Stroke newStroke;
                    newStroke.pattern = MrDoc::solidLinePattern;
                    QStringRef color = attributes.value("", "color");
                    newStroke.color = stringToColor(color.toString());
                    QStringRef style = attributes.value("", "style");
                    if (style.toString().compare("solid") == 0)
                    {
                        newStroke.pattern = MrDoc::solidLinePattern;
                    }
                    else if (style.toString().compare("dash") == 0)
                    {
                        newStroke.pattern = MrDoc::dashLinePattern;
                    }
                    else if (style.toString().compare("dashdot") == 0)
                    {
                        newStroke.pattern = MrDoc::dashDotLinePattern;
                    }
                    else if (style.toString().compare("dot") == 0)
                    {
                        newStroke.pattern = MrDoc::dotLinePattern;
                    } else {
                        newStroke.pattern = MrDoc::solidLinePattern;
                    }
                    QStringRef strokeWidth = attributes.value("", "width");
                    newStroke.penWidth = strokeWidth.toDouble();
                    QString elementText = reader.readElementText();
                    QStringList elementTextList = elementText.trimmed().split(" ");
                    for (int i = 0; i+1 < elementTextList.size(); i = i + 2)
                    {
                        newStroke.points.append(QPointF(elementTextList.at(i).toDouble(), elementTextList.at(i+1).toDouble()));
                    }
                    QStringRef pressures = attributes.value("pressures");
                    QStringList pressuresList = pressures.toString().trimmed().split(" ");
                    for (int i = 0; i < pressuresList.length(); ++i)
                    {
                        if (pressuresList.length() == 0)
                        {
                            newStroke.pressures.append(1.0);
                        } else {
                            newStroke.pressures.append(pressuresList.at(i).toDouble());
                        }
                    }
                    if (newStroke.pressures.size() != newStroke.points.size())
                    {
                        return false;
                    }
                    pages.last().m_strokes.append(newStroke);
                    strokeCount++;
                    qDebug() << strokeCount;
                }
            }
        }

        QFileInfo fileInfo(file);
        file.close();

        if (reader.hasError())
        {
            return false;
        } else {
            path = fileInfo.absolutePath();
            docName = fileInfo.completeBaseName();
            return true;
        }
    }

    bool Document::saveMOJ(QString fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }

        QXmlStreamWriter writer;

        writer.setAutoFormatting(true);

        writer.setDevice(&file);

        writer.writeStartDocument("1.0", false);
        writer.writeStartElement("MrWriter");
        QString version;
        version.append(QString::number(MAJOR_VERSION)).append(".").append(QString::number(MINOR_VERSION));
        writer.writeAttribute(QXmlStreamAttribute("version", version));
        QString docVersion = QString::number(DOC_VERSION);
        writer.writeAttribute(QXmlStreamAttribute("docversion", docVersion));

        writer.writeStartElement("title");
        writer.writeCharacters("MrWriter document - see http://unruhschuh.com/mrwriter/");
        writer.writeEndElement();

        for (int i = 0; i < pages.size(); ++i)
        {
            writer.writeStartElement("page");
            writer.writeAttribute(QXmlStreamAttribute("width" , QString::number(pages[i].getWidth())));
            writer.writeAttribute(QXmlStreamAttribute("height", QString::number(pages[i].getHeight())));
            writer.writeEmptyElement("background");
            writer.writeAttribute(QXmlStreamAttribute("type", "solid"));
            writer.writeAttribute(QXmlStreamAttribute("color", toRGBA(pages[i].backgroundColor.name(QColor::HexArgb))));
            writer.writeAttribute(QXmlStreamAttribute("style", "plain"));
            writer.writeStartElement("layer");

            for (int j = 0; j < pages[i].m_strokes.size(); ++j)
            {
                writer.writeStartElement("stroke");
                writer.writeAttribute(QXmlStreamAttribute("tool", "pen"));
                writer.writeAttribute(QXmlStreamAttribute("color", toRGBA(pages[i].m_strokes[j].color.name(QColor::HexArgb))));
                QString patternString;
                if (pages[i].m_strokes[j].pattern == MrDoc::solidLinePattern)
                {
                    patternString = "solid";
                }
                else if (pages[i].m_strokes[j].pattern == MrDoc::dashLinePattern)
                {
                    patternString = "dash";
                }
                else if (pages[i].m_strokes[j].pattern == MrDoc::dashDotLinePattern)
                {
                    patternString = "dashdot";
                }
                else if (pages[i].m_strokes[j].pattern == MrDoc::dotLinePattern)
                {
                    patternString = "dot";
                } else {
                    patternString = "solid";
                }
                writer.writeAttribute(QXmlStreamAttribute("style", patternString));
                qreal width = pages[i].m_strokes[j].penWidth;
                writer.writeAttribute(QXmlStreamAttribute("width", QString::number(width)));
                QString pressures;
                for (int k = 0; k < pages[i].m_strokes[j].pressures.length(); ++k)
                {
                    pressures.append(QString::number(pages[i].m_strokes[j].pressures[k])).append(" ");
                }
                writer.writeAttribute((QXmlStreamAttribute("pressures", pressures.trimmed())));
                QString points;
                for (int k = 0; k < pages[i].m_strokes[j].points.size(); ++k)
                {
                    points.append(QString::number(pages[i].m_strokes[j].points[k].x()));
                    points.append(" ");
                    points.append(QString::number(pages[i].m_strokes[j].points[k].y()));
                    points.append(" ");
                }
                writer.writeCharacters(points.trimmed());
                writer.writeEndElement(); // closing "stroke"
            }

            writer.writeEndElement(); // closing "layer"
            writer.writeEndElement(); // closing "page"
        }

        writer.writeEndDocument();

        QFileInfo fileInfo(file);

        file.close();

        if (writer.hasError())
        {
            return false;
        } else {
            setDocumentChanged(false);


            path = fileInfo.absolutePath();
            docName = fileInfo.completeBaseName();
            return true;
        }

    }

    bool Document::setDocName(QString newDocName)
    {
        // check for special characters not to be used in filenames ... (probably not)
        docName = newDocName;
        return true;
    }

    QString Document::getDocName()
    {
        return docName;
    }

    bool Document::setPath(QString newPath)
    {
        path = newPath;
        return true;
    }

    QString Document::getPath()
    {
        return path;
    }

    bool Document::getDocumentChanged()
    {
        return documentChanged;
    }

    void Document::setDocumentChanged(bool changed)
    {
        documentChanged = changed;
    }

    QString Document::toARGB(QString rgba)
    {
        // #RRGGBBAA
        // 012345678
        QString argb;
        if (rgba.length() == 9)
        {
            argb.append('#');
            argb.append(rgba.mid(7,2));
            argb.append(rgba.mid(1,6));
        } else {
            argb = QString("");
        }

        return argb;
    }

    QString Document::toRGBA(QString argb)
    {
        // #AARRGGBB
        // 012345678
        QString rgba;
        if (argb.length() == 9)
        {
            rgba.append('#');
            rgba.append(argb.mid(3,6));
            rgba.append(argb.mid(1,2));
        } else {
            rgba = QString("");
        }

        return rgba;
    }

    QColor Document::stringToColor(QString colorString)
    {
        QColor color;
        if (colorString.left(1).compare("#") == 0)
        {
            color = QColor(toARGB(colorString));
        } else {
            for (int i = 0; i < standardColors.size(); ++i)
            {
                if (standardColorNames[i].compare(colorString) == 0)
                {
                    color = standardColors.at(i);
                }
            }
        }
        return color;
    }
}
