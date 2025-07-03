import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { retryWhen, delay, mergeMap, timeout, catchError, map, switchMap } from 'rxjs/operators';
import { Observable, Subscriber, timer, throwError } from 'rxjs'; // throwError moved here
import { HttpErrorResponse } from '@angular/common/http'; // Import HttpErrorResponse

export interface Item {
  id: number;
  name: string;
}

export interface SVConfig {
  appID: string;
  macAddress: string;
  interface: string;
  svid: string;
  scenariofile: string;
  gocbRef: string,
  datSet: string,
  goID: string,
  MAC: string,
  APPID: string,
  interface_Goose: string
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
  private apiUrl = 'http://localhost:3000/api';
  private readonly TIMEOUT_MS = 5000; // 5 seconds timeout
  private readonly MAX_RETRIES = 3;
  private readonly BACKOFF_FACTOR = 2;

  constructor(private http: HttpClient) {}


   private handleHttpError(error: HttpErrorResponse) {
    let errorMessage = 'An unknown error occurred!';

    // Check if it's a client-side error (e.g., network error, CORS)
    // In a browser, error.error would be an ErrorEvent for client-side errors.
    // In Node.js (SSR), ErrorEvent is not defined, so we check for error.error being an actual Error object.
    if (error.error instanceof Error) { // Check if it's a generic Error object
      // A client-side or network error occurred.
      errorMessage = `Client-side error: ${error.error.message}`;
    } else if (error.status) {
      // The backend returned an unsuccessful response code.
      // The response body may contain clues as to what went wrong.
      errorMessage = `Server-side error: ${error.status} ${error.statusText || ''} - ${JSON.stringify(error.error)}`;
    } else {
      // No error.error, and no status, likely a network issue where no response was received.
      errorMessage = `Network error: Could not connect to the server.`;
    }
    
    console.error(errorMessage);
    return throwError(() => new Error(errorMessage));
  }
private getRetryStrategy<T>() {
    return retryWhen<T>(errors =>
      errors.pipe(
        mergeMap((error, index) => {
          // Check if it's a TimeoutError from the RxJS timeout operator
          // The 'name' property is a standard part of JavaScript Error objects.
          if (error.name === 'TimeoutError') {
            if (index < this.MAX_RETRIES) {
              const delayMs = this.BACKOFF_FACTOR ** index * 1000;
              console.log(`Retry attempt ${index + 1} in ${delayMs}ms due to TimeoutError`);
              return timer(delayMs);
            } else {
              // Max retries reached for TimeoutError
              return throwError(() => new Error(`Max retries (${this.MAX_RETRIES}) reached for TimeoutError.`));
            }
          } else if (error instanceof HttpErrorResponse) {
            // Handle other HttpErrorResponse errors (e.g., 500, 404)
            // If you want to retry on specific HTTP status codes, add logic here.
            // For now, we'll re-throw non-timeout HttpErrorResponses immediately.
            console.error(`Not retrying for HttpErrorResponse with status ${error.status}`);
            return throwError(() => error);
          } else {
            // Handle any other unexpected errors
            console.error(`Not retrying for unexpected error:`, error);
            return throwError(() => error);
          }
        })
      )
    );
  }



   getItems(): Observable<Item[]> {
    console.log('ItemService: Fetching items');
    return this.http.get<Item[]>(`${this.apiUrl}/items` ).pipe(
      timeout(this.TIMEOUT_MS),
      this.getRetryStrategy<Item[]>(), // Specify the type here
      catchError(this.handleHttpError)
    );
  }


  verifyConfig(config: SVConfig): Observable<any> {
    console.log('ItemService: Verifying config:', config);
    return this.http.post(`${this.apiUrl}/verify-config`, config).pipe(
      timeout(this.TIMEOUT_MS),
      this.getRetryStrategy<any>(), 
      catchError(this.handleHttpError)
    );
  }

  startSimulation(config: SVConfig): Observable<any> {
    console.log('ItemService: Starting simulation:', config);
    return this.http.post(`${this.apiUrl}/start-simulation`, config).pipe(
      timeout(this.TIMEOUT_MS),
       this.getRetryStrategy<any>(), 
      catchError(this.handleHttpError)
    );
  }

  stopSimulation(): Observable<any> {
    console.log('ItemService: stopping simulation:');
    return this.http.post(`${this.apiUrl}/stop-simulation`, "").pipe(
      timeout(this.TIMEOUT_MS),
      this.getRetryStrategy<any>(), 
      catchError(this.handleHttpError)
    );
  }
  
  verifyGooseConfig(config: GooseConfig): Observable<any> {
    console.log('ItemService: Verifying GOOSE config:', config);
    return this.http.post(`${this.apiUrl}/verify-goose-config`, config).pipe(
      timeout(this.TIMEOUT_MS),
       this.getRetryStrategy<any>(), 
      catchError(this.handleHttpError)
    );
  }

  SendGooseMessage(config: any): Observable<any> {
    console.log('ItemService: Sending GOOSE message:', config);
    return this.http.post(`${this.apiUrl}/send-goose-message`, config).pipe(
      timeout(this.TIMEOUT_MS),
      this.getRetryStrategy<any>(), 
      catchError(this.handleHttpError)
    );
  }
}