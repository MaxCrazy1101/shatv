#pragma once

#include <QDialog>

namespace shatv::ui::windows {

class AboutDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

}  // namespace shatv::ui::windows
