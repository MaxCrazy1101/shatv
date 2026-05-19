#pragma once

#include <QAbstractListModel>
#include <vector>

#include "domain/channel.h"

namespace shatv::ui::models {

class ChannelListModel final : public QAbstractListModel {
    Q_OBJECT

   public:
    enum Roles {
        kIdRole = Qt::UserRole + 1,
        kNameRole,
        kGroupRole,
        kUrlRole,
        kCurrentRole,
    };

    explicit ChannelListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void SetChannels(std::vector<domain::Channel> channels);
    void SetCurrentChannelId(const QString &channel_id);

    domain::Channel ChannelAt(const QModelIndex &index) const;
    QString CurrentChannelId() const;

   private:
    int FindRowByChannelId(const QString &channel_id) const;

    std::vector<domain::Channel> channels_;
    QString current_channel_id_;
};

}  // namespace shatv::ui::models
