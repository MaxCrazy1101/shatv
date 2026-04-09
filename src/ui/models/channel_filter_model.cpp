#include "ui/models/channel_filter_model.h"

#include <QSet>

#include "ui/models/channel_list_model.h"

namespace shatv::ui::models {

ChannelFilterModel::ChannelFilterModel(QObject *parent) : QSortFilterProxyModel(parent) {}

QStringList ChannelFilterModel::AvailableGroups() const {
    if (sourceModel() == nullptr) {
        return {};
    }

    QStringList groups;
    QSet<QString> seen_groups;
    const int total_rows = sourceModel()->rowCount();
    for (int row = 0; row < total_rows; ++row) {
        const QString group =
            sourceModel()->index(row, 0).data(ChannelListModel::kGroupRole).toString().trimmed();
        if (group.isEmpty() || seen_groups.contains(group)) {
            continue;
        }
        seen_groups.insert(group);
        groups.push_back(group);
    }
    return groups;
}

void ChannelFilterModel::SetGroupFilter(const QString &group) {
    const QString normalized_group = group.trimmed();
    if (group_filter_ == normalized_group) {
        return;
    }

    beginFilterChange();
    group_filter_ = normalized_group;
    endFilterChange(Direction::Rows);
}

void ChannelFilterModel::SetSearchText(const QString &search_text) {
    const QString normalized_search = search_text.trimmed();
    if (search_text_ == normalized_search) {
        return;
    }

    beginFilterChange();
    search_text_ = normalized_search;
    endFilterChange(Direction::Rows);
}

QString ChannelFilterModel::GroupFilter() const {
    return group_filter_;
}

bool ChannelFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (sourceModel() == nullptr) {
        return true;
    }

    // 分组过滤只影响左侧可见项，不改变底层频道数据。
    if (!group_filter_.isEmpty()) {
        const QString group = sourceModel()
                                  ->index(source_row, 0, source_parent)
                                  .data(ChannelListModel::kGroupRole)
                                  .toString()
                                  .trimmed();
        if (group != group_filter_) {
            return false;
        }
    }

    return MatchesSearch(source_row, source_parent);
}

bool ChannelFilterModel::MatchesSearch(int source_row, const QModelIndex &source_parent) const {
    if (sourceModel() == nullptr || search_text_.isEmpty()) {
        return true;
    }

    const QString channel_name = sourceModel()
                                     ->index(source_row, 0, source_parent)
                                     .data(ChannelListModel::kNameRole)
                                     .toString();
    return channel_name.contains(search_text_, Qt::CaseInsensitive);
}

}  // namespace shatv::ui::models
