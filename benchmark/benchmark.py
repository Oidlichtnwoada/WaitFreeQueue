#!/usr/bin/env python3
from matplotlib.pyplot import savefig, close, show
from pandas import options, read_csv
import seaborn as sns

options.mode.chained_assignment = None

# generating latency and throughput plots of the enqueue and dequeue operation
df = read_csv('../benchmark.csv')
df['PERCENTAGE'] = df['ENQ_THREADS'] / df['number of threads']

keys = [('ENQ_LAT', 'enqueue latency [ns]'),
        ('DEQ_LAT', 'dequeue latency [ns]'),
        ('ENQ_THROUGH', 'enqueue throughput [MOPS]'),
        ('DEQ_THROUGH', 'dequeue throughput [MOPS]')]

dfscatter = df.query('ENQ_LAT >= 0 and DEQ_LAT >= 0')
figure = sns.scatterplot(x=dfscatter['ENQ_LAT'], y=dfscatter['DEQ_LAT'], hue=dfscatter['QUEUE'])
figure.set(xlabel='enqueue latency [ns]', ylabel='dequeue latency [ns]')
figure.set_yscale('log')
figure.set_xscale('log')
savefig(f'SCATTER_LAT.pdf')
close()

dfscatter = df.query('ENQ_THROUGH >= 0 and DEQ_THROUGH >= 0')
figure = sns.scatterplot(x=dfscatter['ENQ_THROUGH'], y=dfscatter['DEQ_THROUGH'], hue=dfscatter['QUEUE'])
figure.set(xlabel='enqueue throughput [MOPS]', ylabel='dequeue throughput [MOPS]')
figure.set_yscale('log')
figure.set_xscale('log')
savefig(f'SCATTER_THROUGH.pdf')
close()

dfscatter = df.query('ENQ_LAT >= 0 and ENQ_THROUGH >= 0')
figure = sns.scatterplot(x=dfscatter['ENQ_LAT'], y=dfscatter['ENQ_THROUGH'], hue=dfscatter['QUEUE'])
figure.set(xlabel='enqueue latency [ns]', ylabel='enqueue throughput [MOPS]')
figure.set_yscale('log')
figure.set_xscale('log')
savefig(f'SCATTER_LAT_THROUGH_ENQ.pdf')
close()

dfscatter = df.query('DEQ_LAT >= 0 and DEQ_THROUGH >= 0')
figure = sns.scatterplot(x=dfscatter['DEQ_LAT'], y=dfscatter['DEQ_THROUGH'], hue=dfscatter['QUEUE'])
figure.set(xlabel='dequeue latency [ns]', ylabel='dequeue throughput [MOPS]')
figure.set_yscale('log')
figure.set_xscale('log')
savefig(f'SCATTER_LAT_THROUGH_DEQ.pdf')
close()

for key, name in keys:
    for percentage, group_percentage in df.groupby('PERCENTAGE'):
        percentage = int(percentage * 100)
        if key.startswith('ENQ') and percentage == 0: continue
        if key.startswith('DEQ') and percentage == 100: continue
        group = group_percentage.groupby(['number of threads', 'QUEUE'])
        means = group[key].mean()
        stds = group[key].std()
        figure = means.unstack().plot.bar(title=f'{name} with {percentage}% enqueuers', yerr=stds.unstack(), capsize=4)
        if key.endswith('LAT'): figure.set_yscale('log')
        savefig(f'{key}_{percentage}.pdf')
        close()

# generating CAS failure plots
dfw = df[df.QUEUE == 'WFQ']
dfw['CAS_RATE_FAIL'] = dfw['WFQ_NUM_CAS_FAILED'] / dfw['WFQ_NUM_CAS'] * 100

for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['CAS_RATE_FAIL'].mean()
    stds = group['CAS_RATE_FAIL'].std()
    figure = means.plot.bar(title=f'CAS failure percentage [%] with {percentage}% enqueuers')
    figure.set_yscale('log')
    savefig(f'CAS_CASFAIL_{percentage}.pdf')
    close()

dfw['PERCENT_SP_ENQ'] = (dfw['WFQ_ENQ_SP']/(dfw['WFQ_ENQ_FP'] + dfw['WFQ_ENQ_SP'])) * 100
for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['PERCENT_SP_ENQ'].mean()
    figure = means.plot.bar(title=f'Slow path enqueues [%] with {percentage}% enqueuers')
    savefig(f'ENQ_SP_{percentage}.pdf')
    close()

dfw['PERCENT_SP_DEQ'] = (dfw['WFQ_DEQ_SP']/(dfw['WFQ_DEQ_FP'] + dfw['WFQ_DEQ_SP'])) * 100
for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['PERCENT_SP_DEQ'].mean()
    figure = means.plot.bar(title=f'Slow path dequeues [%] with {percentage}% enqueuers')
    savefig(f'DEQ_SP_{percentage}.pdf')
    close()

for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['WFQ_ENQ_SLOW_DIJKSTRA'].mean()
    figure = means.plot.bar(title=f'Enqueue Dijkstra protocol loop iterations with {percentage}% enqueuers')
    savefig(f'ENQ_DIJKSTRA_{percentage}.pdf')
    close()

for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['WFQ_ENQ_SLOW_LIN'].mean()
    figure = means.plot.bar(title=f'Enqueue linearization loop iterations with {percentage}% enqueuers')
    savefig(f'ENQ_LINEARIZATION_{percentage}.pdf')
    close()

for percentage, group_percentage in dfw.groupby('PERCENTAGE'):
    percentage = int(percentage * 100)
    group = group_percentage.groupby(['number of threads'])
    means = group['WFQ_DEQ_HELPER'].mean()
    figure = means.plot.bar(title=f'Dequeue helping loop iterations with {percentage}% enqueuers')
    savefig(f'DEQ_HELP_{percentage}.pdf')
    close()