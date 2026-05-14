'use strict';

const Homey = require('homey');

class MSpaDriver extends Homey.Driver {
  async onPairListDevices() {
    return [{
      name: 'MSpa HTTP Controller',
      data: { id: `mspa-${Date.now()}` },
      settings: {
        api_host: ''
      }
    }];
  }
}

module.exports = MSpaDriver;
