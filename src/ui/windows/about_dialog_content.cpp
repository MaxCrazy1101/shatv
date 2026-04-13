#include "ui/windows/about_dialog_content.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "app/build_info.h"

namespace shatv::ui::windows {

namespace {

QString Tr(const char *source_text) {
    return QCoreApplication::translate("shatv::ui::windows::AboutDialogContent", source_text);
}

}  // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(Tr("About ShaTV"));
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    // App name
    auto *name_label = new QLabel(Tr("ShaTV"), this);
    name_label->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: bold;"));
    layout->addWidget(name_label);

    // Description
    auto *desc_label = new QLabel(Tr("A cross-platform IPTV player."), this);
    layout->addWidget(desc_label);

    layout->addSpacing(4);

    // Version / Build info
    auto *form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->addRow(Tr("Version"), new QLabel(QString::fromUtf8(app::kProjectVersion), this));
    form->addRow(Tr("Build"), new QLabel(QString::fromUtf8(app::kBuildId), this));
    layout->addLayout(form);

    layout->addSpacing(4);

    // Tech stack
    auto *tech_label = new QLabel(Tr("Built with C++20, Qt 6, and libmpv."), this);
    tech_label->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(tech_label);

    layout->addSpacing(8);

    // Author
    auto *author_heading = new QLabel(QStringLiteral("<b>%1</b>").arg(Tr("Authors")), this);
    layout->addWidget(author_heading);
    auto *author_label = new QLabel(
        QStringLiteral("MaxCrazy (<a href=\"mailto:alex02newton@gmail.com\">alex02newton@gmail.com</a>)"), this);
    author_label->setOpenExternalLinks(true);
    layout->addWidget(author_label);

    layout->addSpacing(4);

    // License
    auto *license_heading = new QLabel(QStringLiteral("<b>%1</b>").arg(Tr("License")), this);
    layout->addWidget(license_heading);
    layout->addWidget(new QLabel(Tr("Not specified"), this));

    layout->addSpacing(4);

    // Repository
    auto *repo_heading = new QLabel(QStringLiteral("<b>%1</b>").arg(Tr("Repository")), this);
    layout->addWidget(repo_heading);
    auto *repo_label =
        new QLabel(QStringLiteral("<a href=\"https://github.com/MaxCrazy1101/shatv\">github.com/MaxCrazy1101/shatv</a>"),
                   this);
    repo_label->setOpenExternalLinks(true);
    layout->addWidget(repo_label);

    layout->addStretch();

    // Close button
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

}  // namespace shatv::ui::windows
