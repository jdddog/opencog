from math import sqrt, fabs
from numpy import PINF as POSITIVE_INFINITY
from scipy.stats import t, uniform
from spatiotemporal.temporal_events import TemporalEvent, BaseRelationFormula
from spatiotemporal.unix_time import UnixTime, random_time
from utility.geometric import FUNCTION_ONE, FunctionLinear, FunctionComposite, FUNCTION_ZERO

__author__ = 'keyvan'


class RelationFormulaTrapezium(BaseRelationFormula):
    def compare(self, dist_1, dist_2):
        a_1, b_1 = self.bounds_of(dist_1)
        a_2, b_2 = self.bounds_of(dist_2)

        dist_1_duration, dist_2_duration = self.duration_of(dist_1), self.duration_of(dist_2)
        dist_1_uniform_probability = 1.0 / dist_1_duration
        dist_2_uniform_probability = 1.0 / dist_2_duration

        same_a = max(a_1, a_2)
        same_b = min(b_1, b_2)

        same = 0
        if same_a < same_b:
            same = sqrt(dist_1_uniform_probability * dist_2_uniform_probability) * (same_b - same_a)

        l = fabs(dist_1_duration - dist_2_duration)

        dictionary_bounds_function = {(l / 2.0, POSITIVE_INFINITY): FUNCTION_ONE}
        if l > 0:
            dictionary_bounds_function[(-l / 2.0, l / 2.0)] = FunctionLinear(x_0=- l / 2.0, y_0=0, x_1=l / 2.0, y_1=1)
        proportion_function = FunctionComposite(dictionary_bounds_function, function_undefined=FUNCTION_ZERO)

        non_same_portion = 1.0 - same
        after = proportion_function(dist_1.mean() - dist_2.mean()) * non_same_portion
        return 1 - same - after, same, after


class TemporalEventTrapezium(TemporalEvent):
    beginning_factor = 5
    ending_factor = 5

    def __init__(self, a, b, beginning=None, ending=None, beginning_factor=None, ending_factor=None):
        """
        start and end can be in either datetime or unix time
        """
        a, b = UnixTime(a), UnixTime(b)
        assert a < b, "'b' should be greater than 'a'"
        if (beginning, ending) != (None, None):
            assert (beginning_factor, ending_factor) == (None, None), "PiecewiseTemporalEvent() only accepts " \
                                                                      "either 'beginning_factor' and 'ending_factor' " \
                                                                      "or 'beginning' and 'ending'"

        if beginning_factor is not None:
            assert beginning_factor > 0
            self.beginning_factor = beginning_factor
        if ending_factor is not None:
            assert ending_factor > 0
            self.ending_factor = ending_factor

        if (beginning, ending) == (None, None):
            beginning, ending = 0, 0
            while not a < beginning < ending < b:
                beginning = random_time(
                    a,
                    b,
                    probability_distribution=t(
                        # df, mean, variance
                        4,
                        a + float(b - a) / self.beginning_factor,
                        float(b - a) / self.beginning_factor
                    )
                )

                ending = random_time(
                    a,
                    b,
                    probability_distribution=t(
                        # df, mean, variance
                        4,
                        b - float(b - a) / self.ending_factor,
                        float(b - a) / self.ending_factor
                    )
                )
        TemporalEvent.__init__(self, uniform(loc=a, scale=UnixTime(beginning - a)),
                               uniform(loc=ending, scale=UnixTime(b - ending)), bins=4,
                               relation_formula=RelationFormulaTrapezium())


def generate_random_events(size=20):
    from datetime import datetime
    from spatiotemporal.time_intervals import TimeInterval
    events = []

    year_2010 = TimeInterval(datetime(2010, 1, 1), datetime(2011, 1, 1))

    for i in xrange(size):
        start = year_2010.random_time()
        end = year_2010.random_time(start)
        event = TemporalEventTrapezium(start, end)
        events.append(event)

    return events


if __name__ == '__main__':
    import time

    #event = TemporalEventTrapezium(1, 20)
    #
    #event.plot()
    #event.distribution_beginning.plot()
    #plt = event.distribution_ending.plot()
    #plt.ylim(ymin=0, ymax=1.1)
    #plt.show()

    #events = generate_random_events(5)
    events = generate_random_events(500)

    start = time.time()

    for event in events:
        plt = event.plot()
        #plt = event.instance().plot()

    print 'Performance:', time.time() - start, 'seconds'

    plt.ylim(ymin=0, ymax=1.1)
    plt.show()
