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
  }
}

module.exports = MSpaHomeyApp;
