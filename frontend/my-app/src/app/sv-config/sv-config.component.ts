import { Component, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ItemService, Item, SVConfig } from '../item.service';
import { HttpClient, HttpClientModule, HttpErrorResponse } from '@angular/common/http';

@Component({
  selector: 'app-sv-config',
  standalone: true,
  imports: [CommonModule, FormsModule,CommonModule, FormsModule, HttpClientModule],
  templateUrl: './sv-config.component.html',
  styleUrls: ['./sv-config.component.css']
})
export class SvConfigComponent implements OnInit {
  title = 'IEC 61850 SV Configuration';
  items: Item[] = [];
  config: SVConfig = {
    appID: '',
    macAddress: '',
    GOid: '',
    interface: '',
    cbref: '',
    svid: '',
    scenariofile: ''
  };
  configStatus: string = '';
  simulationStatus: string = '';
  isConfigValid: boolean = false;

  constructor(private itemService: ItemService) {}

  ngOnInit() {
    console.log('SvConfigComponent: Initializing...');
    this.itemService.getItems().subscribe({
      next: (items: Item[]) => {
        console.log('SvConfigComponent: Items fetched:', items);
        this.items = items;
      },
      error: (err: HttpErrorResponse) => {
        console.error('SvConfigComponent: Fetch items error:', err);
        this.configStatus = 'Error fetching items: ' + err.message;
      }
    });
  }

  startConfig() {
    console.log('SvConfigComponent: Starting config:', this.config);
    this.configStatus = 'Verifying...';
    this.itemService.verifyConfig(this.config).subscribe({
      next: (response: any) => {
        console.log('SvConfigComponent: Verify config response:', response);
        if ('message' in response) {
          this.configStatus = response.message;
          this.isConfigValid = response.message === 'Configuration is valid';
          if (this.isConfigValid) {
            setTimeout(() => {
              this.configStatus = '';
            }, 10000);
          }
        } else if ('errors' in response) {
          this.configStatus = 'Errors: ' + response.errors.join(', ');
          this.isConfigValid = false;
        }
      },
      error: (err: HttpErrorResponse) => {
        console.error('SvConfigComponent: Verify config error:', err);
        const errorMessage = err.error?.errors?.join(', ') || err.message;
        this.configStatus = 'Verification failed: ' + errorMessage;
        this.isConfigValid = false;
      }
    });
  }

  startSimulation() {
    console.log('SvConfigComponent: Starting simulation:', this.config);
    this.simulationStatus = 'Starting simulation...';
    this.itemService.startSimulation(this.config).subscribe({
      next: (response: any) => {
        console.log('SvConfigComponent: Start simulation response:', response);
        if ('message' in response) {
          this.simulationStatus = response.message;
          setTimeout(() => {
            this.simulationStatus = '';
          }, 20000);
        } else if ('error' in response) {
          this.simulationStatus = 'Error: ' + response.error;
          setTimeout(() => {
            this.simulationStatus = '';
          }, 20000);
        }
      },
      error: (err: HttpErrorResponse) => {
        console.error('SvConfigComponent: Start simulation error:', err);
        const errorMessage = err.error?.error || err.message;
        this.simulationStatus = 'Simulation failed: ' + errorMessage;
        setTimeout(() => {
          this.simulationStatus = '';
        }, 20000);
      }
    });
  }

  stopSimulation() {
    console.log('SvConfigComponent: Stopping simulation');
    this.simulationStatus = 'Stopping simulation...';
    this.itemService.stopSimulation().subscribe({
      next: (response: any) => {
        console.log('SvConfigComponent: stop simulation response:', response);
        if ('message' in response) {
          this.simulationStatus = response.message;
          setTimeout(() => {
            this.simulationStatus = '';
          }, 20000);
        } else if ('error' in response) {
          this.simulationStatus = 'Error: ' + response.error;
          setTimeout(() => {
            this.simulationStatus = '';
          }, 20000);
        }
      },
      error: (err: HttpErrorResponse) => {
        console.error('SvConfigComponent: stop simulation error:', err);
        const errorMessage = err.error?.error || err.message;
        this.simulationStatus = 'Simulation failed: ' + errorMessage;
        setTimeout(() => {
          this.simulationStatus = '';
        }, 20000);
      }
    });
  }

  onFileSelected(event: Event) {
    console.log('SvConfigComponent: File selected');
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      const file = input.files[0];
      if (!file.name.endsWith('.xml')) {
        this.configStatus = 'Error: Please upload a valid XML file';
        this.isConfigValid = false;
        return;
      }

      const reader = new FileReader();
      reader.onload = () => {
        try {
          const xmlStr = reader.result as string;
          const parser = new DOMParser();
          const xmlDoc = parser.parseFromString(xmlStr, 'application/xml');
          const errorNode = xmlDoc.querySelector('parsererror');
          if (errorNode) {
            throw new Error('Invalid XML format');
          }
          const xmlConfig = xmlDoc.querySelector('SVConfig');
          if (!xmlConfig) {
            throw new Error('Invalid XML structure: Missing SVConfig root element');
          }

          this.config = {
            appID: xmlConfig.querySelector('appID')?.textContent || '',
            macAddress: xmlConfig.querySelector('macAddress')?.textContent || '',
            GOid: xmlConfig.querySelector('GOid')?.textContent || '',
            interface: xmlConfig.querySelector('interface')?.textContent || '',
            cbref: xmlConfig.querySelector('cbref')?.textContent || '',
            svid: xmlConfig.querySelector('svid')?.textContent || '',
            scenariofile: xmlConfig.querySelector('scenariofile')?.textContent || ''
          };
          this.configStatus = 'XML config loaded successfully';
          console.log('SvConfigComponent: XML config loaded:', this.config);
          this.startConfig();
        } catch (error: any) {
          console.error('SvConfigComponent: Error parsing XML:', error);
          this.configStatus = 'Error parsing XML: ' + error.message;
          this.isConfigValid = false;
        }
      };
      reader.onerror = () => {
        console.error('SvConfigComponent: Error reading file');
        this.configStatus = 'Error reading file';
        this.isConfigValid = false;
      };
      reader.readAsText(file);
    }
  }
}