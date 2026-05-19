#include "player/retry_schedule.h"

#include <QtTest>

namespace {

using shatv::domain::RetryBackoff;
using shatv::domain::RetryPolicy;
using shatv::player::RetryDecisionForFailure;

class RetryScheduleTest final : public QObject {
    Q_OBJECT

   private slots:
    void fail_fast_when_policy_has_no_retries();
    void fixed_backoff_retries_until_max_attempts();
    void increasing_backoff_is_capped_by_max_delay();
};

void RetryScheduleTest::fail_fast_when_policy_has_no_retries() {
    const auto decision = RetryDecisionForFailure(RetryPolicy{}, 1);
    QVERIFY(!decision.should_retry);
    QCOMPARE(decision.next_attempt, 0);
    QCOMPARE(decision.delay_ms, 0);
}

void RetryScheduleTest::fixed_backoff_retries_until_max_attempts() {
    const RetryPolicy policy{
        .max_attempts = 2,
        .initial_delay_ms = 300,
        .max_delay_ms = 1000,
        .backoff = RetryBackoff::kFixed,
    };

    const auto first_failure = RetryDecisionForFailure(policy, 1);
    QVERIFY(first_failure.should_retry);
    QCOMPARE(first_failure.next_attempt, 2);
    QCOMPARE(first_failure.delay_ms, 300);

    const auto second_failure = RetryDecisionForFailure(policy, 2);
    QVERIFY(!second_failure.should_retry);
}

void RetryScheduleTest::increasing_backoff_is_capped_by_max_delay() {
    const RetryPolicy policy{
        .max_attempts = 5,
        .initial_delay_ms = 300,
        .max_delay_ms = 700,
        .backoff = RetryBackoff::kIncreasing,
    };

    const auto first_failure = RetryDecisionForFailure(policy, 1);
    QVERIFY(first_failure.should_retry);
    QCOMPARE(first_failure.next_attempt, 2);
    QCOMPARE(first_failure.delay_ms, 300);

    const auto second_failure = RetryDecisionForFailure(policy, 2);
    QVERIFY(second_failure.should_retry);
    QCOMPARE(second_failure.next_attempt, 3);
    QCOMPARE(second_failure.delay_ms, 600);

    const auto third_failure = RetryDecisionForFailure(policy, 3);
    QVERIFY(third_failure.should_retry);
    QCOMPARE(third_failure.next_attempt, 4);
    QCOMPARE(third_failure.delay_ms, 700);
}

}  // namespace

QTEST_GUILESS_MAIN(RetryScheduleTest)

#include "retry_schedule_test.moc"
