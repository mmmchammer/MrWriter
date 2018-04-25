#ifndef WIDGET_H
#define WIDGET_H

#define MIN_ZOOM 0.1
#define MAX_ZOOM 10.0

#include <QWidget>
#include <QOpenGLWidget>
#include <QVector>
#include <QPainter>
#include <QTabletEvent>
#include <QUndoStack>
#include <QScrollArea>
#include <QSizeGrip>
#include <QGridLayout>
#include <QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <math.h>

#include <QTime>
#include <QTimer>

#include <poppler-link.h>

#include "tabletapplication.h"
#include "mrdoc.h"
#include "document.h"
#include "textbox.h"
#include "markdownbox.h"
#include "page.h"

/**
 * These structs are basically for @ref basePixmapMap
 */
struct BasicPageSize;
struct BasicPageSizeHash;
struct BasicPageSizeEqual;

class Widget : public QWidget
// class Widget : public QOpenGLWidget
{
  Q_OBJECT
public:
  explicit Widget(QWidget *parent = 0);

  enum class tool
  {
    NONE,
    PEN,
    HIGHLIGHTER,
    RULER,
    CIRCLE,
    ERASER,
    SELECT,
    HAND,
    TEXT,
    MARKDOWN
  };
  enum class state
  {
    IDLE,
    DRAWING,
    RULING,
    CIRCLING,
    TEXTTYPING,
    MARKDOWNTYPING,
    SELECTING,
    SELECTED,
    MOVING_SELECTION,
    RESIZING_SELECTION,
    ROTATING_SELECTION
  };
  enum class view
  {
      HORIZONTAL,
      VERTICAL
  };

  static constexpr qreal veryFinePenWidth = 0.42;
  static constexpr qreal finePenWidth = 0.85;
  static constexpr qreal mediumPenWidth = 1.41;
  static constexpr qreal thickPenWidth = 2.26;
  static constexpr qreal veryThickPenWidth = 5.67;

  void setCurrentTool(tool toolID);
  tool getCurrentTool()
  {
    return currentTool;
  }

  void setCurrentPenWidth(qreal penWidth);

  qreal getCurrentPenWidth()
  {
    return currentPenWidth;
  }

  void mouseAndTabletEvent(QPointF mousePos, Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers keyboardModifiers,
                           QTabletEvent::PointerType pointerType, QEvent::Type eventType, qreal pressure, bool tabletEvent);

  /**
   * @brief updateAllPageBuffers updates the page buffers in @ref pageBufferPtr.
   * @details If zoom level changes or a new document was loaded the page buffer
   * is cleared. After that only the visible pages (and a few more pages around)
   * are painted and loaded to @ref pageBufferPtr. For the other pages only a pointer to
   * a blank placeholder (stored in @ref basePixmapMap) is loaded into the buffer.
   */
  void updateAllPageBuffers();
  /**
   * @brief updateNecessaryPagesBuffer updates the page buffer only for currentpage plus/minus 6 pages.
   * @details A page gets repainted if its current buffer pixmap is a placeholder.
   */
  void updateNecessaryPagesBuffer();
  /**
   * @brief updateBuffer updates the page buffer for a single page.
   * @param i page index of the page to repaint and load into buffer
   * @see updateBufferWithPlaceholder
   */
  void updateBuffer(int i);
  /**
   * @brief updateBufferWithPlaceholder loads a pointer to a blank placeholder into @ref pageBufferPtr
   * @param buffNum page index
   */
  void updateBufferWithPlaceholder(int buffNum);
  void updateBufferRegion(int buffNum, QRectF const &clipRect);
  void drawOnBuffer(bool last = false);
  int getPageFromMousePos(QPointF mousePos);
  QPointF getPagePosFromMousePos(QPointF mousePos, int pageNum);
  QPointF getAbsolutePagePosFromMousePos(QPointF mousePos);
  QRect getWidgetGeometry();
  int getCurrentPage();
  /**
   * @brief getVisiblePages
   * @return the number of visible pages
   */
  int getVisiblePages();

  void setCurrentState(state newState);
  state getCurrentState();

  void setCurrentView(view newView);
  view getCurrentView();

  void setCurrentColor(QColor newColor);
  QColor getCurrentColor();

  void setCurrentFont(QFont newFont);
  QFont getCurrentFont();

  void setDocument(const MrDoc::Document &newDocument);
  void letGoSelection();

  void newFile();
  //    void openFile();

  void zoomIn();
  void zoomOut();
  void zoomTo(qreal newZoom);
  void zoomFitWidth();
  void zoomFitHeight();

  void rotateSelection(qreal angle);

  /**
   * @brief closeTextBox closes the text insertion box if one is open
   */
  void closeTextBox();

  /**
   * @brief searchAllPdf searchs in the loaded pdf for @param text and highlights the results.
   */
  void searchAllPdf(const QString &text);

  MrDoc::Document currentDocument;

  QVector<std::shared_ptr<std::shared_ptr<QPixmap>>> pageBufferPtr; /**< buffer for page pixmaps */
  QMutex pageBufferPtrMutex; /**< Mutex for @ref pageBufferPtr */

  QColor currentColor;
  qreal currentPenWidth;

  qreal m_currentAngle;

  void setCurrentPattern(QVector<qreal> newPattern);
  QVector<qreal> getCurrentPattern();

  MrDoc::Selection currentSelection;
  MrDoc::Selection &clipboard = static_cast<TabletApplication *>(qApp)->clipboard;

  int selectingOnPage;

  QScrollArea *scrollArea;

  QUndoStack undoStack;

  qreal zoom;
  qreal prevZoom;

private:
  struct BasicPageSize{
      int pageWidth;
      int pageHeight;
  };
  struct BasicPageSizeHash{
      std::size_t operator ()(const BasicPageSize& s) const{
          std::hash<int> f;
          return f(s.pageHeight) ^ f(s.pageWidth);
      }
  };
  struct BasicPageSizeEqual{
      bool operator ()(const BasicPageSize& a, const BasicPageSize& b) const{
          return a.pageWidth == b.pageWidth && a.pageHeight == b.pageHeight;
      }
  };
  std::unordered_map<BasicPageSize, std::shared_ptr<std::shared_ptr<QPixmap>>, BasicPageSizeHash, BasicPageSizeEqual> basePixmapMap; /**< Stores the pointers to the blank placeholders for certain page sizes */
  QMutex basePixmapMutex;
  int previousVerticalValueRendered = 0; /**< Stores the vertical slider value where the last call of updateAllPageBuffers happened */
  int previousVerticalValueMaybeRendered = 0; /**< Stores the vertical slider value where the last @ref scrollTimer start happened */
  int previousHorizontalValueRendered = 0;
  int previousHorizontalValueMaybeRendered = 0;
  QTimer* scrollTimer;

  QTime timer;

  QTimer *updateTimer;
  QTimer *updateDirtyTimer;

  qreal count;

  QList<int> pageHistory; /**< Stores the page numbers where the user were when he clicked on goto links in the pdf */
  int pageHistoryPosition = -1; /**< Stores the current position in @ref pageHistory */
  void appendToPageHistory(int pageNum); /**< Appends a new page to @ref pageHistory and truncates unnecessary page numbers */
  void scrollDocumentToPageNum(int pageNum);

  QVector<qreal> currentPattern = MrDoc::solidLinePattern;

  QCursor penCursor;
  QCursor circleCursor;
  QCursor rulerCursor;
  QCursor eraserCursor;

  MrDoc::Stroke currentStroke;
  QRect currentUpdateRect;

  state currentState;

  bool penDown = false;
  int drawingOnPage;

  tool currentTool;
  tool previousTool;
  bool realEraser;

  view currentView;

  QFont currentFont;

  qreal currentDashOffset;

  qreal minWidthMultiplier = 0.0;
  qreal maxWidthMultiplier = 1.25;

  QPointF currentCOSPos;
  QPointF firstMousePos;
  QPointF previousMousePos;
  QPointF previousPagePos;
  MrDoc::Selection::GrabZone m_grabZone = MrDoc::Selection::GrabZone::None;

  TextBox* textBox;
  bool textBoxOpen = false;
  bool textChanged = false;

  MarkdownBox* markdownBox;
  bool markdownBoxOpen = false;
  bool markdownChanged = false;

  QString previousSearchText;
  int previousSearchPageIndex;
  QVector<int> searchPageNums; /**< Stores the page numbers where a search result is */

  void startDrawing(QPointF mousePos, qreal pressure);
  void continueDrawing(QPointF mousePos, qreal pressure);
  void stopDrawing(QPointF mousePos, qreal pressure);

  void startRuling(QPointF mousePos);
  void continueRuling(QPointF mousePos);
  void stopRuling(QPointF mousePos);

  void startCircling(QPointF mousePos);
  void continueCircling(QPointF mousePos);
  void stopCircling(QPointF mousePos);

  void startSelecting(QPointF mousePos);
  void continueSelecting(QPointF mousePos);
  void stopSelecting(QPointF mousePos);

  void startMovingSelection(QPointF mousePos);
  void continueMovingSelection(QPointF mousePos);

  void startRotatingSelection(QPointF mousePos);
  void continueRotatingSelection(QPointF mousePos);
  void stopRotatingSelection(QPointF mousePos);

  void startResizingSelection(QPointF mousePos, MrDoc::Selection::GrabZone grabZone);
  void continueResizingSelection(QPointF mousePos);
  void stopResizingSelection(QPointF mousePos);

  void setPreviousTool();

  void erase(QPointF mousePos, bool invertEraser = false);

private slots:
  /**
   * @brief updatePageAfterText updates page buffer when text is inserted
   * @param i page index
   */
  void updatePageAfterText(int i);
  /**
   * @brief updatePage starts @ref scrollTimer when the user scrolled more than one (average) page height/width
   * @param value is current scrollbar value
   */
  void updatePageAfterScrolling(int value);
  void updateAllDirtyBuffers();
  /**
   * @brief updatePageAfterScrollTimer updates (necessary) pages when @ref scrollTimer stopped.
   */
  void updatePageAfterScrollTimer();

  void undo();
  void redo();

  void selectAll();
  void copy();
  void paste();
  void cut();

  void pageFirst();
  void pageLast();
  void pageUp();
  void pageDown();

  void pageAddBefore();
  void pageAddAfter();
  void pageAddBeginning();
  void pageAddEnd();

  void pageRemove();

  void veryFine();
  void fine();
  void medium();
  void thick();
  void veryThick();

  void solidPattern();
  void dashPattern();
  void dashDotPattern();
  void dotPattern();

  void updateWhileDrawing();

signals:
  void pen();
  void ruler();
  void circle();
  void eraser();
  void select();
  void hand();
  void setSimpleText();
  void setMarkdownText();

  void updateGUI();

  void modified();

protected:
  void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

  void tabletEvent(QTabletEvent *event) Q_DECL_OVERRIDE;

  void keyPressEvent(QKeyEvent* event) override;

  void wheelEvent(QWheelEvent* event) override;

signals:

public slots:
  void searchPdfNext(const QString& text);
  void searchPdfPrev(const QString& text);
  void clearPdfSearch();

  void pageHistoryForward();
  void pageHistoryBackward();
};

#endif // WIDGET_H
