#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

namespace shatv::ui::models {

class ChannelFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT

   public:
    explicit ChannelFilterModel(QObject *parent = nullptr);

    QStringList AvailableGroups() const;
    void SetGroupFilter(const QString &group);
    QString GroupFilter() const;

   protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

   private:
    QString group_filter_;
};

}  // namespace shatv::ui::models
