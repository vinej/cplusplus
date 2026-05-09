#include "MainWindow.h"

#include "AiAssistantWidget.h"
#include "AnalysisWidget.h"
#include "PortfolioWidget.h"
#include "RiskWidget.h"

#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Qt Finance");
    resize(1100, 700);

    auto* analysis = new AnalysisWidget(this);
    connect(analysis, &AnalysisWidget::statusMessage,
            this, [this](const QString& msg) { statusBar()->showMessage(msg); });

    auto* tabs = new QTabWidget(this);
    tabs->addTab(analysis,                  "Analysis");
    tabs->addTab(new PortfolioWidget(this), "Portfolio");
    tabs->addTab(new RiskWidget(this),      "Risk");
    tabs->addTab(new AiAssistantWidget(this), "AI Assistant");

    setCentralWidget(tabs);
    statusBar()->showMessage("Ready");
}
