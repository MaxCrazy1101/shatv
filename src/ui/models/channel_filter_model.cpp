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

QString ChannelFilterModel::GroupFilter() const {
    return group_filter_;
}

bool ChannelFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (sourceModel() == nullptr || group_filter_.isEmpty()) {
        return true;
    }

    // 分组过滤只影响左侧可见项，不改变底层频道数据。
    const QString group = sourceModel()
                              ->index(source_row, 0, source_parent)
                              .data(ChannelListModel::kGroupRole)
                              .toString()
                              .trimmed();
    return group == group_filter_;
}

}  // namespace shatv::ui::models
