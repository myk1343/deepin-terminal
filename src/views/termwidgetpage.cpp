#include "termwidgetpage.h"
#include "termwidget.h"
#include "settings.h"
#include "utils.h"
#include "service.h"
#include "windowsmanager.h"

#include <DLog>
#include <DDialog>

#include <QUuid>
#include <QVBoxLayout>
#include <QApplication>

TermWidgetPage::TermWidgetPage(TermProperties properties, QWidget *parent)
    : QWidget(parent), m_findBar(new PageSearchBar(this))
{
    //qDebug() << "parentTermWidgetPage" << parentWidget();
    m_MainWindow = static_cast<MainWindow *>(parentWidget());
    setFocusPolicy(Qt::NoFocus);
    setProperty("TAB_CUSTOM_NAME_PROPERTY", false);
    // 生成唯一 pageID
    setProperty("TAB_IDENTIFIER_PROPERTY", QUuid::createUuid().toString());

    TermWidget *w = createTerm(properties);
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(w);
    setLayout(m_layout);
    //qDebug() << "w->parent()" << w->parent();

    // Init find bar.
    m_findBar->move(this->x() - 100, this->y() - 100);
    connect(m_findBar, &PageSearchBar::findNext, this, &TermWidgetPage::handleFindNext);
    connect(m_findBar, &PageSearchBar::findPrev, this, &TermWidgetPage::handleFindPrev);
    connect(m_findBar, &PageSearchBar::keywordChanged, this, [ = ](QString keyword) {
        handleUpdateSearchKeyword(keyword);
    });
    connect(DApplicationHelper::instance(), &DApplicationHelper::themeTypeChanged, this, [ = ]() {
        applyTheme();
    });

    /******** Modify by nt001000 renfeixiang 2020-05-27:修改 增加参数区别remove和purge卸载命令 2020-05-28 优化代码 Begin***************/
    connect(this, &TermWidgetPage::uninstallTerminal, this, [this](QString commandname) {
        //MainWindow *mainWindow = qobject_cast<MainWindow *>(parent);
        //构造函数中已经获取了mainwindow窗口，无需在获取一遍
        if (m_MainWindow->hasRunningProcesses()) {
            if (!Utils::showExitUninstallConfirmDialog()) {
                return false;
            }
        }
        return Utils::showUnistallConfirmDialog(commandname);
    });
    /******** Modify by nt001000 renfeixiang 2020-05-27:修改 增加参数区别remove和purge卸载命令 Begin***************/

    m_currentTerm = w;
}

MainWindow *TermWidgetPage::parentMainWindow()
{
    //qDebug()<<"parentMainWindow" <<parentWidget();
    return m_MainWindow;
}

void TermWidgetPage::setSplitStyle(DSplitter *splitter)
{
    splitter->setHandleWidth(1);
    QSplitterHandle *handle = splitter->handle(1);

    if (handle) {
        //分割线颜色暂时设置为Highlight颜色，需要和UI确认下
        DPalette pa = DApplicationHelper::instance()->palette(handle);
        QColor splitBrush = pa.color(DPalette::Highlight);
        pa.setBrush(DPalette::Background, splitBrush);
        handle->setPalette(pa);
        handle->setBackgroundRole(QPalette::Background);
        handle->setAutoFillBackground(true);
    }
}

TermWidget *TermWidgetPage::currentTerminal()
{
    return m_currentTerm;
}

//TermWidget *TermWidgetPage::split(Qt::Orientation orientation)
//{
//    return split(currentTerminal(), orientation);
//}
/*******************************************************************************
 1. @函数:
 2. @作者:    ut000439 王培利
 3. @日期:    2020-05-14
 4. @说明:
*******************************************************************************/
void TermWidgetPage::split(Qt::Orientation orientation)
{
    parentMainWindow()->showPlugin(MainWindow::PLUGIN_TYPE_NONE);
    TermWidget *term = m_currentTerm;
    if (getTerminalCount() == 1) {
        qDebug() << "first split";
        QSplitter *firstSplit = createSubSplit(term, orientation);
        m_layout->addWidget(firstSplit);
        return ;
    }

    qDebug() << "not first split";
    QSplitter *upSplit = qobject_cast<QSplitter *>(term->parent());
    int index = upSplit->indexOf(term);
    QList<int> parentSizes = upSplit->sizes();

    // 用新的Split分割布局替换原来的位置
    QSplitter *subSplit = createSubSplit(term, orientation);
    upSplit->insertWidget(index, subSplit);
    upSplit->setSizes(parentSizes);
    setSplitStyle(upSplit);

    return ;
}
/*******************************************************************************
 1. @函数:    createSubSplit
 2. @作者:    ut000439 王培利
 3. @日期:    2020-05-14
 4. @说明:    创建分屏
*******************************************************************************/
DSplitter *TermWidgetPage::createSubSplit(TermWidget *term, Qt::Orientation orientation)
{
    TermProperties properties(term->workingDirectory());
    term->setParent(nullptr);
    TermWidget *newTerm  = createTerm(properties);

    // 意义与名称是相反的
    DSplitter *subSplit = new DSplitter(orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal,
                                        this);
    subSplit->setFocusPolicy(Qt::NoFocus);
    subSplit->insertWidget(0, term);
    subSplit->insertWidget(1, newTerm);
    subSplit->setSizes({ 1, 1 });
    setSplitStyle(subSplit);
    setCurrentTerminal(newTerm);
    return subSplit;
}
/*******************************************************************************
 1. @函数:
 2. @作者:    ut000439 王培利
 3. @日期:    2020-05-15
 4. @说明:
*******************************************************************************/
void TermWidgetPage::closeSplit(TermWidget *term, bool hasConfirmed)
{
    qDebug() << "TermWidgetPage::closeSplit:" << term->getSessionId();
    if (getTerminalCount() > 1) {
        if (!hasConfirmed && term->hasRunningProcess()) {
            showExitConfirmDialog(Utils::CloseType_Terminal, 1, parentMainWindow());
            return;
        }
        QSplitter *upSplit = qobject_cast<QSplitter *>(term->parent());
        term->setParent(nullptr);

        // 另一个兄弟也可能是终端，也可能是split,
        QWidget *brother = upSplit->widget(0);
        TermWidget *nextTerm =  upSplit->findChild<TermWidget *>();
        // 如果上级是分屏
        if ("QSplitter" == QString(upSplit->parent()->metaObject()->className())) {
            QSplitter *upupSplit = qobject_cast<QSplitter *>(upSplit->parent());
            //兄弟替换parent split
            upupSplit->replaceWidget(upupSplit->indexOf(upSplit), brother);
        }
        // 上级不是分屏控件，就是布局在控制了
        else {
            qDebug() << "TermWidgetPage only one term exist!";
            m_layout->addWidget(brother);
        }

        // 子控件的变化会引起焦点的变化，控制焦点要放在最后
        if (nextTerm != nullptr) {
            qDebug() << "nextTerm change" << m_currentTerm->getSessionId();
            nextTerm->setFocus();
        }

        // 释放控件
        term->deleteLater();
        upSplit->setParent(nullptr);
        upSplit->deleteLater();
        qDebug() << "page terminal count =" << getTerminalCount();
        return;
    }
    parentMainWindow()->closeTab(identifier());
    return;
}

void TermWidgetPage::showExitConfirmDialog(Utils::CloseType type, int count, QWidget *parent)
{
    /******** Modify by ut001000 renfeixiang 2020-06-03:修改 直接调用MainWindow中的showExitConfirmDialog函数中 Begin***************/
    parentMainWindow()->showExitConfirmDialog(type, count, parent);
    /******** Modify by ut001000 renfeixiang 2020-06-03:修改 直接调用MainWindow中的showExitConfirmDialog函数中 End***************/
//    // count < 1 不提示
//    if (count < 1) {
//        return;
//    }
//    QString title;
//    QString txt;
//    Utils::getExitDialogText(type, title, txt, count);

//    // 有弹窗显示
//    Service::instance()->setIsDialogShow(window(), true);

//    DDialog *dlg = new DDialog(title, txt, parent);
//    dlg->setIcon(QIcon::fromTheme("deepin-terminal"));
//    dlg->addButton(QString(tr("Cancel")), false, DDialog::ButtonNormal);
//    /******** Modify by nt001000 renfeixiang 2020-05-21:修改Exit成Close Begin***************/
//    dlg->addButton(QString(tr("Close")), true, DDialog::ButtonWarning);
//    /******** Modify by nt001000 renfeixiang 2020-05-21:修改Exit成Close End***************/
//    dlg->setWindowModality(Qt::WindowModal);
//    setAttribute(Qt::WA_ShowModal);
//    dlg->show();


//    if (type == Utils::CloseType_Terminal) {
//        connect(dlg, &DDialog::finished, this, [this](int result) {
//            qDebug() << result;
//            // 有弹窗消失
//            Service::instance()->setIsDialogShow(window(), false);
//            if (result == 1) {
//                //接口二次重入
//                closeSplit(currentTerminal(), true);
//            }
//        });
//    }

//    if (type == Utils::CloseType_OtherTerminals) {
//        connect(dlg, &DDialog::finished, this, [this](int result) {
//            qDebug() << result;
//            // 有弹窗消失
//            Service::instance()->setIsDialogShow(window(), false);
//            if (result == 1) {
//                //接口二次重入
//                closeOtherTerminal(true);
//            }
//        });
//    }

    return ;
}

const QString TermWidgetPage::identifier()
{
    return property("TAB_IDENTIFIER_PROPERTY").toString();
}

void TermWidgetPage::focusCurrentTerm()
{
    m_currentTerm->setFocus();
}

void TermWidgetPage::closeOtherTerminal(bool hasConfirmed)
{
    int runningCount = runningTerminalCount() - static_cast<int>(currentTerminal()->hasRunningProcess());
    if (!hasConfirmed && runningCount != 0) {
        showExitConfirmDialog(Utils::CloseType_OtherTerminals, runningCount, parentMainWindow());
        return;
    }

    QList<TermWidget *> termList = findChildren<TermWidget *>();

    int currSessionId = m_currentTerm->getSessionId();
    //exit protection
    for (TermWidget *term : qAsConst(termList)) {
        if (term->getSessionId() != currSessionId) {
            // 前面已经检测过了，不用重复检测
            closeSplit(term, true);
        }
    }

    setTerminalOpacity(Settings::instance()->opacity());
}

#if 0
// 待删除
typedef struct {
    QPoint topLeft;
    QPoint middle;
    QPoint bottomRight;
} CoordinateRect;

// 待删除
static void transpose(QPoint *point)
{
    int x = point->x();
    point->setX(point->y());
    point->setY(x);
}

// 待删除
static void transposeTransform(CoordinateRect *point)
{
    transpose(&point->topLeft);
    transpose(&point->middle);
    transpose(&point->bottomRight);
}

// 待删除
static void flipTransform(CoordinateRect *point)
{
    QPoint oldTopLeft = point->topLeft;
    point->topLeft = -(point->bottomRight);
    point->bottomRight = -(oldTopLeft);
    point->middle = -(point->middle);
}

// 待删除
static void normalizeToRight(CoordinateRect *point, Qt::Edge dir)
{
    switch (dir) {
    case Qt::LeftEdge:
        flipTransform(point);
        break;
    case Qt::RightEdge:
        // No-op
        break;
    case Qt::TopEdge:
        flipTransform(point);
        transposeTransform(point);
        break;
    case Qt::BottomEdge:
        transposeTransform(point);
        break;
    default:
        qFatal("Invalid navigation");
        return;
    }
}
#endif

/******** Modify by ut000439 wangpeili 2020-01-07:  修改了计算上下左右判断的方法 ********×****/
void TermWidgetPage::focusNavigation(Qt::Edge dir)
{
    QPoint comparPoint = GetComparePoint(currentTerminal(), dir);
    TermWidget *dst = nullptr;
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    QMap<TermWidget *, QRect> mapTermRect;
    for (TermWidget *term : qAsConst(termList)) {
        if (GetRect(term).contains(comparPoint)) {
            qDebug() << "yes!" << comparPoint.x() << comparPoint.y();
            dst = term;
            break;
        }
    }
    if (dst) {
        dst->setFocus();
    }
}
/*******************************************************************************
 1. @函数:    getTerminalCount
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-21
 4. @说明:    获取当前页的终端个数
*******************************************************************************/
int TermWidgetPage::getTerminalCount()
{
    return findChildren<TermWidget *>().size();
}
/********************* Modify by ut000439 wangpeili End ************************/

/*******************************************************************************
 1. @函数:  QRect TermWidgetPage::GetRect(TermWidgetWrapper *term)
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:  获取标签页内所有子窗体的区域
*******************************************************************************/
QRect TermWidgetPage::GetRect(TermWidget *term)
{
    QPoint leftTop = term->mapTo(term->window(), QPoint(0, 0));
    QPoint rightBottom = term->mapTo(term->window(), QPoint(term->width(), term->height()));
    QRect rec(leftTop, rightBottom);
    qDebug() << "leftTop: " << leftTop.x() << leftTop.y() << "rightBottom: " << rightBottom.x() << rightBottom.y();
    return rec;
}

/*******************************************************************************
 1. @函数: QPoint TermWidgetPage::GetComparePoint(TermWidgetWrapper *term, Qt::Edge dir)
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-08
 4. @说明:     获取窗口上下左右键需要判断的点位信息
*******************************************************************************/
QPoint TermWidgetPage::GetComparePoint(TermWidget *term, Qt::Edge dir)
{
    QPoint leftTop = term->mapTo(term->window(), QPoint(0, 0));
    QPoint leftBottom = term->mapTo(term->window(), QPoint(0, term->height()));
    QPoint rightTop = term->mapTo(term->window(), QPoint(term->width(), 0));
    QPoint ret;
    switch (dir) {
    case Qt::LeftEdge:
        ret = leftTop + QPoint(-1, 1);
        break;
    case Qt::RightEdge:
        ret = rightTop + QPoint(1, 1);
        break;
    case Qt::TopEdge:
        ret = leftTop + QPoint(1, -1);
        break;
    case Qt::BottomEdge:
        ret = leftBottom + QPoint(1, 1);
        break;
    default:
        qFatal("Invalid navigation");
        break;
    }
    return ret;
}

int TermWidgetPage::runningTerminalCount()
{
    int count = 0;
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        if (term->hasRunningProcess()) {
            count++;
        }
    }
    return count;
}

TermProperties TermWidgetPage::createCurrentTerminalProperties()
{
    TermProperties properties;

    TermWidget *term = currentTerminal();
    if (term) {
        properties[WorkingDir] = currentTerminal()->workingDirectory();
    }

    return properties;
}

void TermWidgetPage::setTerminalOpacity(qreal opacity)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setTermOpacity(opacity);
    }
}

void TermWidgetPage::setColorScheme(const QString &name)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setColorScheme(name);
    }
}

void TermWidgetPage::sendTextToCurrentTerm(const QString &text)
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->sendText(text);
    }
}

void TermWidgetPage::copyClipboard()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->copyClipboard();
    }
}

/*******************************************************************************
 1. @函数: void TermWidgetPage::pasteClipboard()
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明: 用户粘贴时候，直接粘贴剪切板内容
*******************************************************************************/
void TermWidgetPage::pasteClipboard()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->pasteClipboard();
    }
}

void TermWidgetPage::toggleShowSearchBar()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->toggleShowSearchBar();
    }
}

void TermWidgetPage::zoomInCurrentTierminal()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->zoomIn();
    }
}

void TermWidgetPage::zoomOutCurrentTerminal()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->zoomOut();
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::setFontSize(int fontSize)
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    设置字体大小
*******************************************************************************/
void TermWidgetPage::setFontSize(int fontSize)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setTermFontSize(fontSize);
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::setFont(QString fontName)
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    设置字体
*******************************************************************************/
void TermWidgetPage::setFont(QString fontName)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setTermFont(fontName);
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::selectAll()
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    全选
*******************************************************************************/
void TermWidgetPage::selectAll()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->setSelectionAll();
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::skipToNextCommand()
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    跳转到下一命令
*******************************************************************************/
void TermWidgetPage::skipToNextCommand()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->skipToNextCommand();
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::skipToPreCommand()
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    跳转到前一命令
*******************************************************************************/
void TermWidgetPage::skipToPreCommand()
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->skipToPreCommand();
    }
}

/*******************************************************************************
 1. @函数:   void TermWidgetPage::setcursorShape()
 2. @作者:     ut000439 王培利
 3. @日期:     2020-01-10
 4. @说明:    设置鼠标形状
*******************************************************************************/
void TermWidgetPage::setcursorShape(int shape)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setCursorShape(shape);
    }
}

/*******************************************************************************
 1. @函数:    setBlinkingCursor(bool enable)
 2. @作者:    王培利ut000439
 3. @日期:    2020-01-10
 4. @说明:    设置鼠标闪烁
*******************************************************************************/
void TermWidgetPage::setBlinkingCursor(bool enable)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setBlinkingCursor(enable);
    }
}

void TermWidgetPage::setPressingScroll(bool enable)
{
    QList<TermWidget *> termList = findChildren<TermWidget *>();
    for (TermWidget *term : termList) {
        term->setPressingScroll(enable);
    }
}
/*******************************************************************************
 1. @函数:    showSearchBar(bool enable)
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    是否显示搜索框
*******************************************************************************/
void TermWidgetPage::showSearchBar(bool enable)
{
    if (enable) {
        /******** Add by nt001000 renfeixiang 2020-05-18:修改雷神窗口太小时，查询界面使用不方便，将雷神窗口变大适应正常的查询界面 Begin***************/
        if (m_MainWindow->isQuakeMode() && m_MainWindow->height() < 220) {
            m_MainWindow->resize(m_MainWindow->width(), 220); //首先设置雷神界面的大小
            m_MainWindow->showPlugin(MainWindow::PLUGIN_TYPE_SEARCHBAR);//重新打开查询界面，当前流程结束
            return;
        }
        /******** Add by nt001000 renfeixiang 2020-05-18:修改雷神窗口太小时，查询界面使用不方便，将雷神窗口变大适应正常的查询界面 End***************/
        m_findBar->raise();
        m_findBar->clearHoldContent();
        m_findBar->show();
        m_findBar->move(width() - 382, 0);
        QTimer::singleShot(10, this, [ = ] { m_findBar->focus(); });
    } else {
        m_findBar->hide();
    }
}

/*******************************************************************************
 1. @函数:    showRenameTitleDialog
 2. @作者:    ut000610 戴正文
 3. @日期:    2020-05-21
 4. @说明:    判断是否有重命名弹窗，有则显示，没有则创建
*******************************************************************************/
void TermWidgetPage::showRenameTitleDialog(QString oldTitle)
{
    // 弹窗显示
    Service::instance()->setIsDialogShow(window(), true);

    if (nullptr == m_renameDialog) {
        m_renameDialog = new TermInputDialog(this);
        m_renameDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_renameDialog, &TermInputDialog::finished, m_renameDialog, [ = ]() {
            // 弹窗隐藏或消失
            Service::instance()->setIsDialogShow(window(), false);
            m_renameDialog = nullptr;
        });
        m_renameDialog->setFixedSize(380, 180);
        m_renameDialog->setIcon(QIcon::fromTheme("deepin-terminal"));
        m_renameDialog->setFocusPolicy(Qt::NoFocus);
        m_renameDialog->showDialog(oldTitle, this);
    }
}

void TermWidgetPage::onTermRequestRenameTab(QString newTabName)
{
    if (newTabName.isEmpty()) {
        setProperty("TAB_CUSTOM_NAME_PROPERTY", false);
        emit termTitleChanged(m_currentTerm->title());
    } else {
        // Mark it as we renamed it.
        setProperty("TAB_CUSTOM_NAME_PROPERTY", true);
        // Yeah, TermWidgetPage doesn't store the tab name, only the tab bar did it.
        emit tabTitleChanged(newTabName);
    }
}

void TermWidgetPage::onTermTitleChanged(QString title) const
{
    TermWidget *term = qobject_cast<TermWidget *>(sender());
    if (m_currentTerm == term) {
        emit termTitleChanged(title);
    }
}

void TermWidgetPage::onTermGetFocus()
{
    TermWidget *term = qobject_cast<TermWidget *>(sender());
    setCurrentTerminal(term);
    emit Service::instance()->currentTermChange(m_currentTerm);
    qDebug() << "onTermGetFocus" << m_currentTerm->getSessionId();
    m_currentTerm->setFocus(Qt::OtherFocusReason);
    emit termGetFocus();
    m_findBar->hide();
}

void TermWidgetPage::onTermClosed()
{
    TermWidget *w = qobject_cast<TermWidget *>(sender());
    if (!w) {
        qDebug() << "TermWidgetPage::onTermClosed: Unknown object to handle" << w;
        Q_ASSERT(0);
    }
    closeSplit(w);
}
/*******************************************************************************
 1. @函数:    handleFindNext
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    查找下一个接口
*******************************************************************************/
void TermWidgetPage::handleFindNext()
{
    qDebug() << m_findBar->searchKeytxt();
    setMismatchAlert(false);
    m_currentTerm->search(m_findBar->searchKeytxt(), true, true);
}

/*******************************************************************************
 1. @函数:    handleFindPrev
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    查找上一个接口
*******************************************************************************/
void TermWidgetPage::handleFindPrev()
{
    setMismatchAlert(false);
    m_currentTerm->search(m_findBar->searchKeytxt(), false, false);
}

void TermWidgetPage::slotFindbarClose()
{
    //    EditWrapper *wrapper = currentWrapper();
    //    if (wrapper->bottomBar()->isHidden())
    //    {
    //        wrapper->bottomBar()->show();
    //    }
}

void TermWidgetPage::handleRemoveSearchKeyword()
{
    //    currentWrapper()->textEditor()->removeKeywords();
}
/*******************************************************************************
 1. @函数:    handleUpdateSearchKeyword
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    更新key(没有输入回车情况),自动查找接口
*******************************************************************************/
void TermWidgetPage::handleUpdateSearchKeyword(const QString &keyword)
{
    setMismatchAlert(false);
    if (keyword.isEmpty()) {
        m_currentTerm->clearSelection();
    } else {
        // 输入时直接查找，被禁用
        // m_currentTerm->search(m_findBar->SearchKeytxt(), true, false);
    }
}
/*******************************************************************************
 1. @函数:    applyTheme
 2. @作者:    ut000439 王培利
 3. @日期:    2020-03-011
 4. @说明:    非DTK控件手动匹配系统主题的修改
*******************************************************************************/
void TermWidgetPage::applyTheme()
{
    updateSplitStyle();
}

void TermWidgetPage::updateSplitStyle()
{
    QList<DSplitter *> splitList = findChildren<DSplitter *>();
    for (DSplitter *splitter : qAsConst(splitList)) {
        setSplitStyle(splitter);
    }
}

void TermWidgetPage::setCurrentTerminal(TermWidget *term)
{
    TermWidget *oldTerm = m_currentTerm;
    m_currentTerm = term;
    if (oldTerm != m_currentTerm) {
        qDebug() << "m_currentTerm change" << m_currentTerm->getSessionId();
        if (m_currentTerm->isTitleChanged()) {
            emit termTitleChanged(m_currentTerm->title());
        } else {
            emit termTitleChanged(windowTitle());
        }
    }
}

TermWidget *TermWidgetPage::createTerm(TermProperties properties)
{
    TermWidget *term = new TermWidget(properties, this);
    connect(term, &TermWidget::termRequestRenameTab, this, &TermWidgetPage::onTermRequestRenameTab);
    connect(term, &TermWidget::termTitleChanged, this, &TermWidgetPage::onTermTitleChanged);
    connect(term, &TermWidget::termGetFocus, this, &TermWidgetPage::onTermGetFocus);
    connect(term, &TermWidget::finished, this, &TermWidgetPage::onTermClosed);
    qDebug() << "create Terminal, sessionId = " << term->getSessionId();
    return term;
}

void TermWidgetPage::setTextCodec(QTextCodec *codec)
{
    TermWidget *term = currentTerminal();
    if (term) {
        term->setTextCodec(codec);
    }
}
/*******************************************************************************
 1. @函数:    setMismatchAlert
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    是否将输入框标为警告
*******************************************************************************/
void TermWidgetPage::setMismatchAlert(bool alert)
{
    m_findBar->setNoMatchAlert(alert);
}
/*******************************************************************************
 1. @函数:    resizeEvent
 2. @作者:    ut000439 王培利
 3. @日期:    2020-02-24
 4. @说明:    重绘时候，调整查找框的位置
*******************************************************************************/
void TermWidgetPage::resizeEvent(QResizeEvent *event)
{
    //qDebug() << "resizeEvent" << x() << y();
    this->m_findBar->move(width() - 382, 0);
}
