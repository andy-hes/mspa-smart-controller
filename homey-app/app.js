'use strict';

const Homey = require('homey');

class MSpaHomeyApp extends Homey.App {
  async onInit() {
    this.log('MSpa Homey app initialized');

    const restoreCard = this.homey.flow.getActionCard('mspa_restore');
    restoreCard.registerRunListener(async (args) => {
      await args.device.callApi('/api/restore', 'POST');
      await args.device.refreshStatus();
      return true;
    });

    const testCard = this.homey.flow.getActionCard('mspa_test_connection');
    testCard.registerRunListener(async (args) => {
      const status = await args.device.callApi('/api/status', 'GET');
      if (!status || status.ok !== true) {
        throw new Error('MSpa API did not return a valid status payload');
      }
      await args.device.refreshStatus();
      return true;
    });
  }
}

module.exports = MSpaHomeyApp;
