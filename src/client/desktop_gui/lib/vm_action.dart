import 'grpc_client.dart';

const actionPastTenses = {
  'Start': 'Started',
  'Stop': 'Stopped',
  'Suspend': 'Suspended',
  'Restart': 'Restarted',
  'Delete': 'Deleted',
  'Recover': 'Recovered',
  'Purge': 'Purged',
};

const actionContinuousTenses = {
  'Start': 'Starting',
  'Stop': 'Stopping',
  'Suspend': 'Suspending',
  'Restart': 'Restarting',
  'Delete': 'Deleting',
  'Recover': 'Recovering',
  'Purge': 'Purging',
};

const actionAllowedStatuses = {
  'Start': {Status.STOPPED, Status.SUSPENDED},
  'Stop': {Status.RUNNING},
  'Suspend': {Status.RUNNING},
  'Restart': {Status.RUNNING},
  'Delete': {Status.STOPPED, Status.SUSPENDED, Status.RUNNING},
  'Recover': {Status.DELETED},
  'Purge': {Status.DELETED},
};

class VmAction {
  final String name;
  final String pastTense;
  final String continuousTense;
  final Iterable<String> vmNames;
  final Set<Status> allowedStatuses;
  final Future<void> Function(Iterable<String>) function;

  VmAction({
    required this.name,
    required this.vmNames,
    required this.function,
  })  : pastTense = actionPastTenses[name]!,
        continuousTense = actionContinuousTenses[name]!,
        allowedStatuses = actionAllowedStatuses[name]!;
}
