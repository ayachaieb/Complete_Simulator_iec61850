import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';

export interface Item {
  id: number;
  name: string;
}

export interface SVConfig {
  appID: string;
  macAddress: string;
  GOid: string;
  interface: string;
  cbref: string;
  svid: string;
  scenariofile: string;
}

export interface GooseConfig {
  gocbRef: string,
  datSet: string,
  goID: string,
  macAddress: string,
  appID: string,
  interface: string
}

@Injectable({
  providedIn: 'root'
})
export class ItemService {
  //private apiUrl = 'http://localhost:3000/api';
  private apiUrl = 'http://10.42.0.68:3000/api';
  constructor(private http: HttpClient) {}

  getItems(): Observable<Item[]> {
    console.log('ItemService: Fetching items');
    return this.http.get<Item[]>(`${this.apiUrl}/items`);
  }

  verifyConfig(config: SVConfig): Observable<any> {
    console.log('ItemService: Verifying config:', config);
    return this.http.post(`${this.apiUrl}/verify-config`, config);
  }

  startSimulation(config: SVConfig): Observable<any> {
    console.log('ItemService: Starting simulation:', config);
    return this.http.post(`${this.apiUrl}/start-simulation`, config);
  }

  
  verifyGooseConfig(config: GooseConfig): Observable<any> {
    console.log('ItemService: Verifying GOOSE config:', config);
    return this.http.post(`${this.apiUrl}/verify-goose-config`, config);
  }
  SendGooseMessage(config: any): Observable<any> {
    console.log('ItemService: Sending GOOSE message:', config);
    return this.http.post(`${this.apiUrl}/send-goose-message`, config);
  }
}