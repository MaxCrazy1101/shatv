#include "ui/models/channel_list_model.h"

namespace shatv::ui::models {

ChannelListModel::ChannelListModel(QObject *parent) : QAbstractListModel(parent) {}

int ChannelListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(channels_.size());
}

QVariant ChannelListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(channels_.size())) {
        return {};
    }

    const domain::Channel &channel = channels_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case kNameRole:
            return channel.name;
        case kIdRole:
            return channel.id;
        case kGroupRole:
            return channel.group;
        case kUrlRole:
            return channel.url.toString();
        case kCurrentRole:
            return channel.id == current_channel_id_;
        default:
            return {};
    }
}

QHash<int, QByteArray> ChannelListModel::roleNames() const {
    return {
        {kIdRole, "channelId"},
        {kNameRole, "channelName"},
        {kGroupRole, "channelGroup"},
        {kUrlRole, "channelUrl"},
        {kCurrentRole, "isCurrent"},
    };
}

void ChannelListModel::SetChannels(std::vector<domain::Channel> channels) {
    beginResetModel();
    channels_ = std::move(channels);
    current_channel_id_.clear();
    endResetModel();
}

void ChannelListModel::SetCurrentChannelId(const QString &channel_id) {
    const int previous_row = FindRowByChannelId(current_channel_id_);
    const int next_row = FindRowByChannelId(channel_id);

    current_channel_id_ = channel_id;

    if (previous_row >= 0) {
        const QModelIndex index = createIndex(previous_row, 0);
        emit dataChanged(index, index, {kCurrentRole});
    }
    if (next_row >= 0) {
        const QModelIndex index = createIndex(next_row, 0);
        emit dataChanged(index, index, {kCurrentRole});
    }
}

domain::Channel ChannelListModel::ChannelAt(const QModelIndex &index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(channels_.size())) {
        return {};
    }
    return channels_[index.row()];
}

QString ChannelListModel::CurrentChannelId() const {
    return current_channel_id_;
}

int ChannelListModel::FindRowByChannelId(const QString &channel_id) const {
    if (channel_id.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        if (channels_[i].id == channel_id) {
            return i;
        }
    }
    return -1;
}

}  // namespace shatv::ui::models
