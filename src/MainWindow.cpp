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

    auto connectStatus = [this](auto* widget) {
        connect(widget, &std::remove_pointer_t<decltype(widget)>::statusMessage,
                this, [this](const QString& msg) { statusBar()->showMessage(msg); });
    };

    auto* analysis  = new AnalysisWidget(this);
    auto* portfolio = new PortfolioWidget(this);
    auto* risk      = new RiskWidget(this);

    connectStatus(analysis);
    connectStatus(portfolio);
    connectStatus(risk);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(analysis,                    "Analysis");
    tabs->addTab(portfolio,                   "Portfolio");
    tabs->addTab(risk,                        "Risk");
    tabs->addTab(new AiAssistantWidget(this), "AI Assistant");

    // Refresh portfolio picker whenever the Risk tab becomes active so that
    // portfolios created in the Portfolio tab are immediately visible.
    connect(tabs, &QTabWidget::currentChanged, this,
            [tabs, risk](int index) {
                if (tabs->widget(index) == risk)
                    risk->loadPortfolios();
            });

    setCentralWidget(tabs);
    statusBar()->showMessage("Ready");
}
