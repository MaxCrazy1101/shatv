#include <QtTest>

#include "domain/channel.h"
#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"

namespace {

using shatv::domain::Channel;
using shatv::ui::models::ChannelFilterModel;
using shatv::ui::models::ChannelListModel;

class ChannelFilterModelTest : public QObject {
    Q_OBJECT

   private slots:
    void available_groups_returns_unique_groups_in_first_seen_order();
    void group_filter_restricts_visible_rows();
    void empty_group_filter_shows_all_rows();
};

void ChannelFilterModelTest::available_groups_returns_unique_groups_in_first_seen_order() {
    ChannelListModel source;
    source.SetChannels({
        Channel{.id = "cctv-1", .name = "CCTV-1", .url = QUrl("https://example.com/cctv1.m3u8"), .group = "新闻"},
        Channel{.id = "cctv-13",
                .name = "CCTV-13",
                .url = QUrl("https://example.com/cctv13.m3u8"),
                .group = "新闻"},
        Channel{.id = "dragon",
                .name = "Dragon TV",
                .url = QUrl("https://example.com/dragon.m3u8"),
                .group = "卫视"},
        Channel{.id = "movie", .name = "Movie", .url = QUrl("https://example.com/movie.m3u8"), .group = ""},
    });

    ChannelFilterModel filter;
    filter.setSourceModel(&source);

    QCOMPARE(filter.AvailableGroups(), QStringList({"新闻", "卫视"}));
}

void ChannelFilterModelTest::group_filter_restricts_visible_rows() {
    ChannelListModel source;
    source.SetChannels({
        Channel{.id = "cctv-1", .name = "CCTV-1", .url = QUrl("https://example.com/cctv1.m3u8"), .group = "新闻"},
        Channel{.id = "dragon",
                .name = "Dragon TV",
                .url = QUrl("https://example.com/dragon.m3u8"),
                .group = "卫视"},
        Channel{.id = "movie", .name = "Movie", .url = QUrl("https://example.com/movie.m3u8"), .group = ""},
    });

    ChannelFilterModel filter;
    filter.setSourceModel(&source);
    filter.SetGroupFilter("新闻");

    QCOMPARE(filter.rowCount(), 1);
    QCOMPARE(filter.index(0, 0).data(ChannelListModel::kIdRole).toString(), QString("cctv-1"));
}

void ChannelFilterModelTest::empty_group_filter_shows_all_rows() {
    ChannelListModel source;
    source.SetChannels({
        Channel{.id = "cctv-1", .name = "CCTV-1", .url = QUrl("https://example.com/cctv1.m3u8"), .group = "新闻"},
        Channel{.id = "dragon",
                .name = "Dragon TV",
                .url = QUrl("https://example.com/dragon.m3u8"),
                .group = "卫视"},
    });

    ChannelFilterModel filter;
    filter.setSourceModel(&source);
    filter.SetGroupFilter("卫视");

    filter.SetGroupFilter("");

    QCOMPARE(filter.rowCount(), 2);
}

}  // namespace

QTEST_GUILESS_MAIN(ChannelFilterModelTest)

#include "channel_filter_model_test.moc"
