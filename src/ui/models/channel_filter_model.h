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
    void SetSearchText(const QString &search_text);
    QString GroupFilter() const;
    QString SearchText() const;

   protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

   private:
    bool MatchesSearch(int source_row, const QModelIndex &source_parent) const;

    QString group_filter_;
    QString search_text_;
};

}  // namespace shatv::ui::models
