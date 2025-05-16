import { Component } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { HttpClient, HttpClientModule, HttpErrorResponse } from '@angular/common/http';
import { ItemService, Item, GooseConfig } from '../item.service';
   @Component({
    selector: 'app-goose',
    standalone: true,
    imports: [CommonModule, FormsModule, HttpClientModule],
    templateUrl: './goose.component.html',
    styleUrls: ['./goose.component.css']
  })
   export class GooseConfigComponent {
     title = 'GOOSE Configuration';
     config: GooseConfig = {
      gocbRef: '',
      datSet: '',
      goID: '',
      macAddress: '',
      appID: '',
      interface: ''
    };

     isConfigValid = false;
     configStatus: string | null = null;
     simulationStatus: string | null = null;
    constructor(private itemService: ItemService) {}
   

     startConfig() {
      console.log('GOOSEConfigComponent: Starting config:', this.config);
      this.configStatus = 'Verifying...';
      this.itemService.verifyGooseConfig(this.config).subscribe({
        next: (response: any) => {
          console.log('GooseConfigComponent: Verify config response:', response);
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
          console.error('GooseConfigComponent: Verify config error:', err);
          const errorMessage = err.error?.errors?.join(', ') || err.message;
          this.configStatus = 'Verification failed: ' + errorMessage;
          this.isConfigValid = false;
        }
      });
    }
    SendGooseMessage(){

      console.log('SendGooseMessageComponent: Starting simulation:', this.config);
      this.simulationStatus = 'Starting simulation...';
      this.itemService.SendGooseMessage(this.config).subscribe({
        next: (response: any) => {
          console.log('SendGooseMessageComponent: Start simulation response:', response);
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
          console.error('SendGooseMessageComponent: Start simulation error:', err);
          const errorMessage = err.error?.error || err.message;
          this.simulationStatus = 'Simulation failed: ' + errorMessage;
          setTimeout(() => {
            this.simulationStatus = '';
          }, 20000);
        }
      });

    }


   

    onFileSelected(event: Event) {
      console.log('SendGooseComponent: File selected');
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
            const xmlConfig = xmlDoc.querySelector('GooseConfig');
            if (!xmlConfig) {
              throw new Error('Invalid XML structure: Missing GooseConfig root element');
            }
  
            this.config = {
              gocbRef: xmlConfig.querySelector('gocbRef')?.textContent || '',
              datSet: xmlConfig.querySelector('datSet')?.textContent || '',
              goID: xmlConfig.querySelector('goID')?.textContent || '',
              macAddress: xmlConfig.querySelector('macAddress')?.textContent || '',
              appID: xmlConfig.querySelector('appID')?.textContent || '',
              interface: xmlConfig.querySelector('interface')?.textContent || '',
            };
            this.configStatus = 'XML config loaded successfully';
            console.log('GooseConfigComponent: XML config loaded:', this.config);
            this.startConfig();
          } catch (error: any) {
            console.error('GooseConfigComponent: Error parsing XML:', error);
            this.configStatus = 'Error parsing XML: ' + error.message;
            this.isConfigValid = false;
          }
        };
        reader.onerror = () => {
          console.error('GooseConfigComponent: Error reading file');
          this.configStatus = 'Error reading file';
          this.isConfigValid = false;
        };
        reader.readAsText(file);
      }
    }
   }