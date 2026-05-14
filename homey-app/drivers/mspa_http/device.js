'use strict';

const Homey = require('homey');

class MSpaDevice extends Homey.Device {
  async onInit() {
    this.pollTimer = null;

    this.registerCapabilityListener('onoff', async (value) => {
      await this.callApi(value ? '/api/heater/on' : '/api/heater/off', 'POST');
      await this.refreshStatus();
    });

    this.registerCapabilityListener('target_temperature', async (value) => {
      const rounded = Math.max(20, Math.min(40, Math.round(value)));
      await this.callApi('/api/target-temperature', 'POST', `value=${rounded}`);
      await this.refreshStatus();
    });

    this.registerCapabilityListener('mspa_filter', async (value) => {
      await this.callApi(value ? '/api/filter/on' : '/api/filter/off', 'POST');
      await this.refreshStatus();
    });

    this.registerCapabilityListener('mspa_bubbles', async (value) => {
      await this.callApi(value ? '/api/bubbles/on' : '/api/bubbles/off', 'POST');
      await this.refreshStatus();
    });

    this.registerCapabilityListener('mspa_auto_restore', async (value) => {
      await this.callApi(value ? '/api/auto-restore/on' : '/api/auto-restore/off', 'POST');
      await this.refreshStatus();
    });

    await this.refreshStatus();
    this.pollTimer = this.homey.setInterval(() => this.refreshStatus().catch((err) => this.error(err)), 10000);
  }

  async onSettings({ newSettings, changedKeys }) {
    if (changedKeys.includes('api_host') || changedKeys.includes('host')) {
      const host = String(newSettings.api_host || newSettings.host || '').trim();
      await this.setStoreValue('fallback_host', host);
    }
  }

  async onDeleted() {
    if (this.pollTimer) {
      this.homey.clearInterval(this.pollTimer);
    }
  }

  async callApi(path, method = 'GET', body = null) {
    const hostSetting = String(this.getSetting('api_host') || this.getSetting('host') || '').trim();
    const fallbackHost = String((await this.getStoreValue('fallback_host')) || '').trim();
    const host = hostSetting || fallbackHost;

    if (!host) {
      throw new Error('Missing MSpa controller host in device settings');
    }

    const normalizedHost = host.replace(/^https?:\/\//i, '');
    const url = `http://${normalizedHost}${path}`;
    const headers = {};

    const init = { method, headers };
    if (body != null) {
      headers['Content-Type'] = 'application/x-www-form-urlencoded';
      init.body = body;
    }

    const res = await fetch(url, init);
    if (!res.ok) {
      const text = await res.text();
      throw new Error(`HTTP ${res.status} ${res.statusText}: ${text}`);
    }

    return res.json();
  }

  async refreshStatus() {
    try {
      const status = await this.callApi('/api/status', 'GET');

      await this.setAvailable();
      await this.setCapabilityValue('alarm_generic', !status.online);

      if (typeof status.current_temperature_c === 'number') {
        await this.setCapabilityValue('measure_temperature', status.current_temperature_c);
      }
      if (typeof status.target_temperature_c === 'number') {
        await this.setCapabilityValue('target_temperature', status.target_temperature_c);
      }
      if (typeof status.heater_on === 'boolean') {
        await this.setCapabilityValue('onoff', status.heater_on);
      }
      if (typeof status.filter_on === 'boolean') {
        await this.setCapabilityValue('mspa_filter', status.filter_on);
      }
      if (typeof status.bubbles_level === 'number') {
        await this.setCapabilityValue('mspa_bubbles', status.bubbles_level > 0);
      }
      if (typeof status.auto_restore_enabled === 'boolean') {
        await this.setCapabilityValue('mspa_auto_restore', status.auto_restore_enabled);
      }
      if (typeof status.bath_status === 'number') {
        await this.setCapabilityValue('mspa_bath_status', String(status.bath_status));
      }
    } catch (err) {
      await this.setUnavailable(err.message);
      throw err;
    }
  }
}

module.exports = MSpaDevice;
