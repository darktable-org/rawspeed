'''Test module to collect google benchmark results.'''
from litsupport import shellcommand
from litsupport import testplan
import json
import lit.Test


# This is largely duplicated/copy-pasted from LLVM
# test-suite/litsupport/modules/microbenchmark.py


def _mutateCommandLine(context, commandline):
    cmd = shellcommand.parse(commandline)
    context.rawfilename = cmd.arguments[0]
    cmd.arguments.append("--benchmark_format=json")
    # We need stdout ourself to get the benchmark json data.
    if cmd.stdout is not None:
        raise Exception("Rerouting stdout not allowed for microbenchmarks")
    benchfile = context.tmpBase + '.bench.json'
    cmd.stdout = benchfile
    context.microbenchfiles.append(benchfile)

    return cmd.toCommandline()


def _mutateScript(context, script):
    return testplan.mutateScript(context, script, _mutateCommandLine)


def _collectMicrobenchmarkTime(context, microbenchfiles):
    for f in microbenchfiles:
        content = context.read_result_file(context, f)
        data = json.loads(content)

        # Create a micro_result for each benchmark
        for benchmark in data['benchmarks']:
            # Name for MicroBenchmark
            benchmarkname = benchmark['name']
            # Drop raw file name from the name we will report.
            assert benchmarkname.startswith(context.rawfilename + '/')
            benchmarkname = benchmarkname[len(context.rawfilename + '/'):]
            # Create Result object with PASS
            microBenchmark = lit.Test.Result(lit.Test.PASS)
            # Report the wall time.
            exec_time_metric = lit.Test.toMetricValue(benchmark['WallTime,s'])
            microBenchmark.addMetric('exec_time', exec_time_metric)
            # Propagate the perf profile to the microbenchmark.
            if hasattr(context, 'profilefile'):
                microBenchmark.addMetric(
                    'profile', lit.Test.toMetricValue(
                        context.profilefile))
            # Add the fields we want
            for field in benchmark.keys():
                if field in ['real_time', 'cpu_time', 'time_unit']:
                    continue
                metric = lit.Test.toMetricValue(benchmark[field])
                microBenchmark.addMetric(field, metric)
            # Add Micro Result
            context.micro_results[benchmarkname] = microBenchmark

    # returning the number of microbenchmarks collected as a metric for the
    # base test
    return ({
        'rsbench': lit.Test.toMetricValue(len(context.micro_results))
    })


def mutatePlan(context, plan):
    context.microbenchfiles = []
    plan.runscript = _mutateScript(context, plan.runscript)
    plan.metric_collectors.append(
        lambda context: _collectMicrobenchmarkTime(context,
                                                   context.microbenchfiles)
    )
