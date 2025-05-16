import { Component } from '@angular/core';
import { CommonModule } from '@angular/common';
//import { HttpClient } from '@angular/common/http';
import { HttpClientModule } from '@angular/common/http'; // Add this
import { SvConfigComponent } from './sv-config/sv-config.component';
import { GooseConfigComponent } from './goose/goose.component';
import { SinusSignalsComponent } from './sinus-signals/sinus-signals.component';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [
    CommonModule,
    HttpClientModule, 
    SvConfigComponent,
    GooseConfigComponent,
    SinusSignalsComponent
  ],
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent {
  activeSection: string = 'sv-config';

  setSection(section: string) {
    console.log('AppComponent: Switching to section:', section);
    this.activeSection = section;
  }
} 